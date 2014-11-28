#include <assert.h>
#include "ping.h"

void testping ();

int main() {
    testping();
    return 0;
}

void testping () {

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

    uint16_t csum = ip_csum((struct ip*)fake_ip, 20);

    assert(csum == 0xB1E6);
}