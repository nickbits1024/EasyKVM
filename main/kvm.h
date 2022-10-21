#ifndef KVM_H
#define KVM_H

#define KVM_NUM_PORTS 2

esp_err_t kvm_init();
esp_err_t kvm_preinit();
void kvm_reset();
void kvm_next_port();
void kvm_set_port(int port);
int kvm_get_port();
void kvm_enable(bool enable);
esp_err_t kvm_load_state();

#endif