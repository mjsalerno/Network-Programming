#include "ODR.h"
#include "debug.h"

char host_ip[INET_ADDRSTRLEN] = "127.0.0.1"; /* for testing on comps w/o eth0 */
char host_name[BUFF_SIZE];
static struct msg_queue data_queue = {0}, rrep_queue = {0};

int main(void) {
    struct odr_msg rrep;
    struct msg_node *curr, *prev;
    int x = 1;

    memset(&rrep, 0, sizeof(rrep));

    craft_rrep(&rrep, "4", "5", 0, 10);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "3", "5", 0, 9);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "7", "5", 0, 8);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "1", "5", 0, 10);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "10", "5", 0, 15);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "4", "10", 0, 15);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "4", "2", 0, 15);
    printf("Storing %d\n", x++);
    queue_store(&rrep);
    craft_rrep(&rrep, "8", "5", 0, 25);
    printf("Storing %d\n", x++);
    queue_store(&rrep);

    curr = rrep_queue.head;
    while(curr != NULL) {
        prev = curr;
        curr = curr->next;
        printf("Dst: %s,  Src: %s\n", prev->msg.dst_ip, prev->msg.src_ip);
        free(prev);
    }

    return 0;
}

/* Can pass NULL for srcip or dstip if not wanted. */
void craft_rrep(struct odr_msg *m, char *srcip, char *dstip, int force_redisc, int num_hops) {
    m->type = T_RREP;
    m->force_redisc = (uint8_t)force_redisc;
    m->broadcast_id = 0;
    if(dstip != NULL) {
        strncpy(m->dst_ip, dstip, INET_ADDRSTRLEN);
    }
    m->dst_port = 0;
    if(srcip != NULL) {
        strncpy(m->src_ip, srcip, INET_ADDRSTRLEN);
    }
    m->src_port = 0;
    m->len = 0;
    m->do_not_rrep = 1;
    m->num_hops = (uint16_t)num_hops;
}


void queue_insert(struct msg_queue *queue, struct msg_node *prev, struct msg_node *new, struct msg_node *curr) {
    if(prev == NULL) {   /* case if before head */
        new->next = queue->head;
        queue->head = new;
    } else {                   /* case if after head*/
        new->next = curr;
        prev->next = new;
    }
}

/**
*   Store the message into the queue, to be sent later (upon getting a route).
*   NOTE: For RREPS: replaces an exisiting RREP if this RREP is better.
*           Sorted by destination. (RREPs sorted by dst, then by src)
*
*   malloc()'s space for |-- odr_msg --|--  data  --|  and then puts it at the
*   end of the queue.
*   RETURNS:    0  if the dst IP was not already in the queue
*               1  if the dst IP was already in the queue.
*/
int queue_store(struct odr_msg *m) {
    struct msg_node *new_node, *curr, *prev;
    struct msg_queue *queue;
    int dst_cmp, src_cmp;
    size_t real_len;
    if(m == NULL) {
        _ERROR("%s", "You're msg queue stuff's NULL! Failing!\n");
        exit(EXIT_FAILURE);
    }
    if(m->type == T_DATA) { /* choose the right queue */
        queue = &data_queue;
    } else if(m->type == T_RREP) {
        queue = &rrep_queue;
    } else {
        _ERROR("Can't store this odr_msg type in a queue: %hhu\n", m->type);
        exit(EXIT_FAILURE);
    }

    new_node = malloc(sizeof(struct msg_node) + m->len); /* alloc the enclosing msg_node */
    if(new_node == NULL) {
        _ERROR("%s", "malloc() returned NULL! Failing!\n");
        exit(EXIT_FAILURE);
    }
    real_len = sizeof(struct odr_msg) + m->len; /* alloc the inner odr_msg */
    memcpy(&new_node->msg, m, real_len);
    new_node->next = NULL;
    _DEBUG("Storing msg in queue: msg Type %hhu\n", m->type);
    curr = queue->head;
    prev = NULL;
    if(curr == NULL) {        /* case if list is empty */
        queue->head = new_node;
        return 0;
    }
    for( ; curr != NULL; prev = curr, curr = curr->next) {
        dst_cmp = strncmp(m->dst_ip, curr->msg.dst_ip, INET_ADDRSTRLEN);
        if(dst_cmp <= 0){
            if(dst_cmp == 0 && m->type == T_RREP) { /* dup RREP deletion!! same dst_ip, so check if same src_ip*/
                src_cmp = strncmp(m->src_ip, curr->msg.src_ip, INET_ADDRSTRLEN);
                if(src_cmp < 0) {
                    queue_insert(queue, prev, new_node, curr);
                    return (dst_cmp == 0);
                } else if(src_cmp == 0) { /* same src and dst ip of RREPs */
                    if(m->num_hops < curr->msg.num_hops) {
                        _INFO("Got a better RREP, dropping old, old_hops: %d, new_hops: %d\n", curr->msg.num_hops, m->num_hops);
                        curr->msg.num_hops = m->num_hops;
                    } else {
                        _INFO("Got a worse RREP, dropping it, stored RREP hops: %d, its hops: %d\n", curr->msg.num_hops, m->num_hops);
                    }
                    free(new_node);
                    return (dst_cmp == 0); /* should always be true (or 1) here */
                }
            }
            queue_insert(queue, prev, new_node, curr);
            return (dst_cmp == 0);
        }
    }
    /* case if last element */
    prev->next = new_node;
    return 0;
}

