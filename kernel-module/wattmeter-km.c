/**
 * @file   meter.c
 * @author Derek Molloy
 * @date   19 April 2015
 * @brief  A kernel module for controlling a meter (or any signal) that is connected to
 * a GPIO. It has full support for interrupts and for sysfs entries so that an interface
 * can be created to the meter or the meter can be configured from Linux userspace.
 * The sysfs entry appears at /sys/tomas/gpio115
 * @see http://www.derekmolloy.ie/
*/
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>       // Required for the GPIO functions
#include <linux/interrupt.h>  // Required for the IRQ code
#include <linux/kobject.h>    // Using kobjects for the sysfs bindings
#include <linux/time.h>       // Using the clock to measure time between meter presses
#define  DEBOUNCE_TIME 200    ///< The default bounce time -- 200ms
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Derek Molloy");
MODULE_DESCRIPTION("A simple Linux GPIO Meter LKM for the BBB");
MODULE_VERSION("0.1");
 
static bool isRising = 1;                   ///< Rising edge is the default IRQ property
module_param(isRising, bool, S_IRUGO);      ///< Param desc. S_IRUGO can be read/not changed
MODULE_PARM_DESC(isRising, " Rising edge = 1 (default), Falling edge = 0");  ///< parameter description
 
static unsigned int gpioMeter = 44;       ///< Default GPIO is 44
module_param(gpioMeter, uint, S_IRUGO);    ///< Param desc. S_IRUGO can be read/not changed
MODULE_PARM_DESC(gpioMeter, " GPIO Meter number (default=115)");  ///< parameter description
 
static unsigned int gpioLED = 45;           ///< Default GPIO is 45
module_param(gpioLED, uint, S_IRUGO);       ///< Param desc. S_IRUGO can be read/not changed
MODULE_PARM_DESC(gpioLED, " GPIO LED number (default=45)");         ///< parameter description
 
static char   gpioName[8] = "gpioXXX";      ///< Null terminated default string -- just in case
static int    irqNumber;                    ///< Used to share the IRQ number within this file
static int    numWattHours = 0;            ///< For information, store the number of meter presses
static bool   ledOn = 0;                    ///< Is the LED on or off? Used to invert its state (off by default)
static bool   isDebounce = 1;               ///< Use to store the debounce state (on by default)
static struct timespec ts_last, ts_current, ts_diff;  ///< timespecs from linux/time.h (has nano precision)
 
/// Function prototype for the custom IRQ handler function -- see below for the implementation
static irq_handler_t  tomasgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);
 
/** @brief A callback function to output the numWattHours variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the number of presses
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t numWattHours_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", numWattHours);
}
 
/** @brief A callback function to read in the numWattHours variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer from which to read the number of presses (e.g., reset to 0).
 *  @param count the number characters in the buffer
 *  @return return should return the total number of characters used from the buffer
 */
static ssize_t numWattHours_store(struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count){
   sscanf(buf, "%du", &numWattHours);
   return count;
}
 
/** @brief Displays if the LED is on or off */
static ssize_t ledOn_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", ledOn);
}
 
/** @brief Displays the last time the meter was pressed -- manually output the date (no localization) */
static ssize_t lastTime_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%.2lu:%.2lu:%.2lu:%.9lu \n", (ts_last.tv_sec/3600)%24,
          (ts_last.tv_sec/60) % 60, ts_last.tv_sec % 60, ts_last.tv_nsec );
}
 
/** @brief Display the time difference in the form secs.nanosecs to 9 places */
static ssize_t diffTime_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%lu.%.9lu\n", ts_diff.tv_sec, ts_diff.tv_nsec);
}
 
/** @brief Displays if meter debouncing is on or off */
static ssize_t isDebounce_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
   return sprintf(buf, "%d\n", isDebounce);
}
 
/** @brief Stores and sets the debounce state */
static ssize_t isDebounce_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
   unsigned int temp;
   sscanf(buf, "%du", &temp);                // use a temp varable for correct int->bool
   gpio_set_debounce(gpioMeter,0);
   isDebounce = temp;
   if(isDebounce) { gpio_set_debounce(gpioMeter, DEBOUNCE_TIME);
      printk(KERN_INFO "TOMAS Meter: Debounce on\n");
   }
   else { gpio_set_debounce(gpioMeter, 0);  // set the debounce time to 0
      printk(KERN_INFO "TOMAS Meter: Debounce off\n");
   }
   return count;
}
 
/**  Use these helper macros to define the name and access levels of the kobj_attributes
 *  The kobj_attribute has an attribute attr (name and mode), show and store function pointers
 *  The count variable is associated with the numWattHours variable and it is to be exposed
 *  with mode 0666 using the numWattHours_show and numWattHours_store functions above
 */
static struct kobj_attribute count_attr = __ATTR(numWattHours, 0664, numWattHours_show, numWattHours_store);
static struct kobj_attribute debounce_attr = __ATTR(isDebounce, 0664, isDebounce_show, isDebounce_store);
 
/**  The __ATTR_RO macro defines a read-only attribute. There is no need to identify that the
 *  function is called _show, but it must be present. __ATTR_WO can be  used for a write-only
 *  attribute but only in Linux 3.11.x on.
 */
static struct kobj_attribute ledon_attr = __ATTR_RO(ledOn);     ///< the ledon kobject attr
static struct kobj_attribute time_attr  = __ATTR_RO(lastTime);  ///< the last time pressed kobject attr
static struct kobj_attribute diff_attr  = __ATTR_RO(diffTime);  ///< the difference in time attr
 
/**  The tomas_attrs[] is an array of attributes that is used to create the attribute group below.
 *  The attr property of the kobj_attribute is used to extract the attribute struct
 */
static struct attribute *tomas_attrs[] = {
      &count_attr.attr,                  ///< The number of meter presses
      &ledon_attr.attr,                  ///< Is the LED on or off?
      &time_attr.attr,                   ///< Time of the last meter press in HH:MM:SS:NNNNNNNNN
      &diff_attr.attr,                   ///< The difference in time between the last two presses
      &debounce_attr.attr,               ///< Is the debounce state true or false
      NULL,
};
 
/**  The attribute group uses the attribute array and a name, which is exposed on sysfs -- in this
 *  case it is gpio115, which is automatically defined in the tomasMeter_init() function below
 *  using the custom kernel parameter that can be passed when the module is loaded.
 */
static struct attribute_group attr_group = {
      .name  = gpioName,                 ///< The name is generated in tomasMeter_init()
      .attrs = tomas_attrs,                ///< The attributes array defined just above
};
 
static struct kobject *tomas_kobj;
 
/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point. In this example this
 *  function sets up the GPIOs and the IRQ
 *  @return returns 0 if successful
 */
static int __init tomasMeter_init(void){
   int result = 0;
   unsigned long IRQflags = IRQF_TRIGGER_RISING;      // The default is a rising-edge interrupt
 
   printk(KERN_INFO "TOMAS Meter: Initializing the TOMAS Meter LKM\n");
   sprintf(gpioName, "gpio%d", gpioMeter);           // Create the gpio115 name for /sys/tomas/gpio115
 
   // create the kobject sysfs entry at /sys/tomas -- probably not an ideal location!
   tomas_kobj = kobject_create_and_add("tomas", kernel_kobj->parent); // kernel_kobj points to /sys/kernel
   //tomas_kobj = kobject_create_and_add("tomas", kernel_kobj); // kernel_kobj points to /sys/kernel
   if(!tomas_kobj){
      printk(KERN_ALERT "TOMAS Meter: failed to create kobject mapping\n");
      return -ENOMEM;
   }
   // add the attributes to /sys/tomas/ -- for example, /sys/tomas/gpio115/numWattHours
   result = sysfs_create_group(tomas_kobj, &attr_group);
   if(result) {
      printk(KERN_ALERT "TOMAS Meter: failed to create sysfs group\n");
      kobject_put(tomas_kobj);                          // clean up -- remove the kobject sysfs entry
      return result;
   }
   getnstimeofday(&ts_last);                          // set the last time to be the current time
   ts_diff = timespec_sub(ts_last, ts_last);          // set the initial time difference to be 0
 
   // Going to set up the LED. It is a GPIO in output mode and will be on by default
   ledOn = true;
   gpio_request(gpioLED, "sysfs");          // gpioLED is hardcoded to 49, request it
   gpio_direction_output(gpioLED, ledOn);   // Set the gpio to be in output mode and on
// gpio_set_value(gpioLED, ledOn);          // Not required as set by line above (here for reference)
   gpio_export(gpioLED, false);             // Causes gpio49 to appear in /sys/class/gpio
                     // the bool argument prevents the direction from being changed
   gpio_request(gpioMeter, "sysfs");       // Set up the gpioMeter
   gpio_direction_input(gpioMeter);        // Set the meter GPIO to be an input
   gpio_set_debounce(gpioMeter, DEBOUNCE_TIME); // Debounce the meter with a delay of 200ms
   gpio_export(gpioMeter, false);          // Causes gpio115 to appear in /sys/class/gpio
                     // the bool argument prevents the direction from being changed
 
   // Perform a quick test to see that the meter is working as expected on LKM load
   printk(KERN_INFO "TOMAS Meter: The meter state is currently: %d\n", gpio_get_value(gpioMeter));
 
   /// GPIO numbers and IRQ numbers are not the same! This function performs the mapping for us
   irqNumber = gpio_to_irq(gpioMeter);
   printk(KERN_INFO "TOMAS Meter: The meter is mapped to IRQ: %d\n", irqNumber);
 
   if(!isRising){                           // If the kernel parameter isRising=0 is supplied
      IRQflags = IRQF_TRIGGER_FALLING;      // Set the interrupt to be on the falling edge
   }
   // This next call requests an interrupt line
   result = request_irq(irqNumber,             // The interrupt number requested
                        (irq_handler_t) tomasgpio_irq_handler, // The pointer to the handler function below
                        IRQflags,              // Use the custom kernel param to set interrupt type
                        "tomas_meter_handler",  // Used in /proc/interrupts to identify the owner
                        NULL);                 // The *dev_id for shared interrupt lines, NULL is okay
   return result;
}
 
/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit tomasMeter_exit(void){
   printk(KERN_INFO "TOMAS Meter: The meter was pressed %d times\n", numWattHours);
   kobject_put(tomas_kobj);                   // clean up -- remove the kobject sysfs entry
   gpio_set_value(gpioLED, 0);              // Turn the LED off, makes it clear the device was unloaded
   gpio_unexport(gpioLED);                  // Unexport the LED GPIO
   free_irq(irqNumber, NULL);               // Free the IRQ number, no *dev_id required in this case
   gpio_unexport(gpioMeter);               // Unexport the Meter GPIO
   gpio_free(gpioLED);                      // Free the LED GPIO
   gpio_free(gpioMeter);                   // Free the Meter GPIO
   printk(KERN_INFO "TOMAS Meter: Goodbye from the TOMAS Meter LKM!\n");
}
 
/** @brief The GPIO IRQ Handler function
 *  This function is a custom interrupt handler that is attached to the GPIO above. The same interrupt
 *  handler cannot be invoked concurrently as the interrupt line is masked out until the function is complete.
 *  This function is static as it should not be invoked directly from outside of this file.
 *  @param irq    the IRQ number that is associated with the GPIO -- useful for logging.
 *  @param dev_id the *dev_id that is provided -- can be used to identify which device caused the interrupt
 *  Not used in this example as NULL is passed.
 *  @param regs   h/w specific register values -- only really ever used for debugging.
 *  return returns IRQ_HANDLED if successful -- should return IRQ_NONE otherwise.
 */
static irq_handler_t tomasgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
   ledOn = !ledOn;                      // Invert the LED state on each meter press
   gpio_set_value(gpioLED, ledOn);      // Set the physical LED accordingly
   getnstimeofday(&ts_current);         // Get the current time as ts_current
   ts_diff = timespec_sub(ts_current, ts_last);   // Determine the time difference between last 2 presses
   ts_last = ts_current;                // Store the current time as the last time ts_last
   printk(KERN_INFO "TOMAS Meter: The meter state is currently: %d\n", gpio_get_value(gpioMeter));
   numWattHours++;                     // Global counter, will be outputted when the module is unloaded
   return (irq_handler_t) IRQ_HANDLED;  // Announce that the IRQ has been handled correctly
}
 
// This next calls are  mandatory -- they identify the initialization function
// and the cleanup function (as above).
module_init(tomasMeter_init);
module_exit(tomasMeter_exit);
