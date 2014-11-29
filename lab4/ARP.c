#include "get_hw_addrs.h"

int main() {
    struct hwa_info* hw_list = NULL;
    struct hwa_ip * mip_head = NULL;
    hw_list = get_hw_addrs();
    keep_eth0(&hw_list, &mip_head);
    print_hw_addrs(hw_list);
    free_hwa_info(hw_list);
    print_hwa_list(mip_head);

    return 0;

}