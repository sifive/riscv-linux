#ifndef EIC7700_SID_CFG_H
#define EIC7700_SID_CFG_H

int eic7700_dynm_sid_enable(int nid);
int eic7700_aon_sid_cfg(struct device *dev);
int eic7700_tbu_power(struct device *dev, bool is_powerUp);

#endif
