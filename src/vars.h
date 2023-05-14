#pragma region base
#define WIFI_SSID "ffjfifjfjfjfjfjf"
#define WIFI_PASSWORD ""
#pragma endregion

#pragma region camera
// 默认5分钟上传一次，可更改（本项目是定时自动上传，如需条件触发上传，在需要上传的时候，调用 take_send_photo() 即可）
#define CAPTURE_INTERVAL 300000
#pragma endregion

#pragma region mail
/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.qiye.aliyun.com"

/** The smtp port e.g.
 * 25  or esp_mail_smtp_port_25
 * 465 or esp_mail_smtp_port_465
 * 587 or esp_mail_smtp_port_587
 */
#define SMTP_PORT esp_mail_smtp_port_465

/* The log in credentials */
#define AUTHOR_EMAIL "noreply@moeci.com"
#define AUTHOR_PASSWORD ""

/* Recipient email address */
#define RECIPIENT_EMAIL "yiyungent@126.com"
#pragma endregion

#pragma region bemfa
#define BEMFA_UID ""
#define BEMFA_TOPIC "esp32camimages"
#define BEMFA_POST_URL "http://images.bemfa.com/upload/v1/upimages.php"
#pragma endregion
