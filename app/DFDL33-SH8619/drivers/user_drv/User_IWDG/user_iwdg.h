#ifndef __USER_IWDG_H__
#define __USER_IWDG_H__

int set_iwdg_timeout(int val);
int user_iwdg_init(void);

void iwdg_entry(void);

void reboot(void);

#endif
