//
//  ja_config.h
//  justapis-c-sdk
//
//  Created by Taha Samad on 1/30/17.
//  Copyright © 2017 Nanoscale. All rights reserved.
//
#ifndef JA_CONFIG_H
#define JA_CONFIG_H

///
/// Build Options
/// ------
///

/// JA_ENABLE_RESPONSE_CACHE
/// 1: Build in support for automatic response caching (default)
/// 0: Do not build in support for automatic response caching
#define JA_ENABLE_RESPONSE_CACHE 1

/// JA_ENABLE_CJSON
/// 1: Provides convenience features for JSON in requests and responses, using cJSON (default)
/// 0: Removes convenience features for JSON in requests and responses.
#define JA_ENABLE_CJSON 1

/// JA_ENABLE_PUBLIC_KEY_PINNING
/// 1: Provides fields and functions for public key pinning (default)
/// 0: Removes fields and function for public key pinning
#define JA_ENABLE_PUBLIC_KEY_PINNING 1

/// JA_ENABLE_MQTT
/// 1: Provides fields and functions for interacting with MQTT Broker (default)
/// 0: Removes fields and function for interacting with MQTT Broker
#define JA_ENABLE_MQTT 1

/// JA_ENABLE_MQTT_WITH_THREADING
/// 1: Provides fields and functions for running MQTT loop on a separate thread. You will need pthread for this feature to be enabled (default)
/// 0: Removes fields and functions for running MQTT loop on a separate thread.
#define JA_ENABLE_MQTT_WITH_THREADING 1

#endif //JA_CONFIG_H