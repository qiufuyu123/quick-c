import "LinkedList.qh"

u8 LinkNodeInit(LinkNode* node,u64 val,u64 next){
    node.this = val;
    node.next = next;
    return 1;
}

u8 LinkNodeAdd(LinkNode* parent, LinkNode* next){
    return 2;
}
