

esp_err_t webserver_get_config(httpd_req_t* req);
esp_err_t webserver_get_api_config(httpd_req_t* req);
esp_err_t webserver_post_api_config(httpd_req_t* req);

esp_err_t webserver_get_kvm(httpd_req_t* req);
esp_err_t webserver_get_kvm1(httpd_req_t* req);
esp_err_t webserver_get_kvm2(httpd_req_t* req);

esp_err_t webserver_get_monitor(httpd_req_t* req);
esp_err_t webserver_post_monitor(httpd_req_t* req);