//
//  mosquitto_config.h
//  justapis-c-sdk-xc
//
//  Created by Taha Samad on 1/30/17.
//  Copyright © 2017 Nanoscale. All rights reserved.
//
#ifndef mosquitto_config_h
#define mosquitto_config_h

#import "ja_config.h"

#if JA_ENABLE_MQTT && JA_ENABLE_MQTT_WITH_THREADING
#define WITH_THREADING 1
#endif

#endif /* mosquitto_config_h */
