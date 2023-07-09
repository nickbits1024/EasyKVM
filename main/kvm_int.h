
#define KVM_STATE_KEY           "kvm_state"
#define KVM_NVS_NAME            "kvm"
#define KVM_PORT_GPIO_NUM       (5)
#define KVM_PORT_GPIO_SEL       (1ull << KVM_PORT_GPIO_NUM)
#define KVM_ENABLE_GPIO_NUM     (6)
#define KVM_ENABLE_GPIO_SEL     (1ull << KVM_ENABLE_GPIO_NUM)
#define KVM_MAX_SYNC_RETRIES    5
#define KVM_HUB_DP_GPIO_NUM      (47)
#define KVM_HUB_DP_GPIO_SEL      (1ull << KVM_HUB_DP_GPIO_NUM)
#define KVM_HUB_DN_GPIO_NUM      (21)
#define KVM_HUB_DN_GPIO_SEL      (1ull << KVM_HUB_DN_GPIO_NUM)


typedef struct kvm_state_t
{
    uint16_t last_input;
} kvm_state_t;

static esp_err_t kvm_save_state();
static void check_timer_callback(void* arg);