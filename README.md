
# Set up

## Create credentials.h
Set up the following in the created file:

```c
#ifndef SECRET_CREDENTIALS_H
#define SECRET_CREDENTIALS_H

#define WIFI_SSID   "my_wifi"
#define WIFI_PASS   "admin123"

#define DEVICE_KEY  "secret_key"

#define PLUTO_URL           "http://192.168.0.105:8080"
#define PLUTO_PAYMENT_API   "/authorize"

#endif
```