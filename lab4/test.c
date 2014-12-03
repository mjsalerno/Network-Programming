#include <assert.h>
#include "ping.h"
#include "ARP.h"
#include "common.h"

void test_csum ();
void test_add_arp();

int main() {
    test_csum();
    test_add_arp();
    return 0;
}

void test_csum() {

    //0x4500 0x003c 0x1c46 0x4000 0x4006 0xb1e6 0xac10 0x0a63 0xac10 0x0a0c -> 0xB1E6

    unsigned short fake_ip[10];

    fake_ip[0] = 0x4500;
    fake_ip[1] = 0x003c;
    fake_ip[2] = 0x1c46;
    fake_ip[3] = 0x4000;
    fake_ip[4] = 0x4006;
    fake_ip[5] = 0x0000; //csum set to zero
    fake_ip[6] = 0xac10;
    fake_ip[7] = 0x0a63;
    fake_ip[8] = 0xac10;
    fake_ip[9] = 0x0a0c;

    uint16_t mycsum = csum(fake_ip, 20);

    assert(mycsum == 0xB1E6);
}

void test_add_arp() {
    int err;
    struct arp_cache* arp_head = NULL;
    struct in_addr sin_addr;

    unsigned char mac1[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};

    err = inet_aton("123.234.213.9", &sin_addr);
    assert(err != 0);

    assert(arp_head == NULL);
    add_arp(&arp_head, sin_addr.s_addr, 1, 0xF, 6, mac1);
    assert(arp_head != NULL);
    assert(arp_head->next == NULL);

    add_arp(&arp_head, sin_addr.s_addr, 1, 0xF, 6, mac1);
    assert(arp_head->next->next == NULL);
}
