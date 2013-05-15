# GDB Macros for easier debugging
#
# vim: set expandtab tabstop=4 shiftwidth=4 autoindent smartindent:
#

define dump_ib_list_t
    set $node = (ib_list_node_t *)((ib_list_t *)$arg0)->head
    set $n = ((ib_list_t *)$arg0)->nelts
    set $i = 0
    printf "LIST: %p length=%u\n", $arg0, $n
    while $i < $n
        printf "[%u] %p\n", $i, $node->data
        set $node = $node->next
        set $i = $i + 1
    end
end
document dump_ib_list_t
  dump_ib_list_t <ib_list_t *>
end

define dump_ib_stream_t
    set $node = (ib_sdata_t *)((ib_stream_t *)$arg0)->head
    set $n = ((ib_stream_t *)$arg0)->nelts
    set $i = 0
    printf "STREAM: %p chunks=%u length=%u\n", $arg0, $n, ((ib_stream_t *)$arg0)->slen
    while $i < $n
    if $node->dlen > 0
            eval "printf \"[%%u] len=%%-5u %%.%us\\n\", $i, $node->dlen, (char *)$node->data", $node->dlen
        else
            printf "[%u] len=%-5u <NULL>\n", $i, $node->dlen
        end
        set $node = $node->next
    set $i = $i + 1
    end
end
document dump_ib_stream_t
    dump_ib_stream_t <ib_stream_t *>
end

define dump_ib_tx_t
    set $tx = (ib_tx_t *)$arg0
    printf "TX: id=%s\n", $tx->id
    printf "request_line: "
    if $tx->request_line && $tx->request_line->raw
        eval "printf \"%%.%us\\n\", (char *)$tx->request_line->raw->data", $tx->request_line->raw->length
    else
        printf "\n"
    end
    printf "request_headers:"
    if $tx->request_header
        set $node = $tx->request_header->head
        set $n = $tx->request_header->size
        set $i = 0
        printf " num=%u\n", $n
        while $i < $n
            eval "printf \"  %%.%us: %%.%us\\n\", (char *)$node->name->data, (char *)$node->value->data", $node->name->length, $node->value->length
            set $node = $node->next
            set $i = $i + 1
        end
    else
        printf "\n"
    end
    printf "request_body: "
    if $tx->request_body
        dump_ib_stream_t $tx->request_body
    else
        printf "\n"
    end
end
document dump_ib_tx_t
    dump_ib_tx_t <ib_tx_t *>
end
