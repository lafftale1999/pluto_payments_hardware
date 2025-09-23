
# Set up

## Create credentials.h
Set up the following in the created file:

```c
#ifndef SECRET_CREDENTIALS_H
#define SECRET_CREDENTIALS_H

#define WIFI_SSID   "admin"
#define WIFI_PASS   "admin123"

#define DEVICE_KEY  "secret_key"

#define SERVER_HOST         "192.168.0.100"
#define PLUTO_URL           "https://192.168.0.100:443"
#define PLUTO_PAYMENT_API   "/device/authorize"

#endif
```