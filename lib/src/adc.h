#ifndef ADC_H
#define ADC_H

int adc_init(void);
void adc_cleanup(void);
int adc_read_voltage(int channel, float *voltage);

#endif
