
#define KVM_STATE_KEY       "kvm_state"
#define KVM_NVS_NAME        "kvm"
#define KVM_PORT_GPIO_SEL   GPIO_SEL_5
#define KVM_PORT_GPIO_NUM   GPIO_NUM_5
#define KVM_ENABLE_GPIO_SEL      GPIO_SEL_6
#define KVM_ENABLE_GPIO_NUM      GPIO_NUM_6

typedef struct kvm_state_t
{
    uint16_t last_input;
} kvm_state_t;

esp_err_t kvm_save_state();