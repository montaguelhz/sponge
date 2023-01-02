#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    if (!_set_syn_flag) {
        if (!header.syn) {
            return;
        }
        _isn = header.seqno;
        _set_syn_flag = true;
    }
    string payload = seg.payload().copy();
    WrappingInt32 seqno = header.syn ? header.seqno + 1 : header.seqno;
    uint64_t checkpoint = stream_out().bytes_written();
    uint64_t index = unwrap(seqno, _isn, checkpoint) - 1;

    _reassembler.push_substring(payload, index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_set_syn_flag) {
        return {};
    }
    size_t ack = stream_out().bytes_written() + 1;
    if (stream_out().input_ended())
        return wrap(ack + 1, _isn);
    else
        return wrap(ack, _isn);
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
