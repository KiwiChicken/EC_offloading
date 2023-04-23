/*
 * Datapacket header
 */
#ifndef DATAPACKET_H
#define DATAPACKET_H

#include "network.h"

// DataPacket and DataAck are subclasses of Packet used by TcpSrc and other flow control protocols.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new DataPacket or DataAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

class DataPacket : public Packet
{
    public:
        typedef uint64_t seq_t;
        virtual ~DataPacket() {}

        inline static DataPacket* newpkt(PacketFlow &flow, route_t &route, seq_t seqno, int size)
        {
            DataPacket *p = _packetdb.allocPacket();

            // The sequence number is the first byte of the packet.
            // This will ID the packet by its last byte.
            p->set(flow, route, size, seqno);
            p->_seqno = seqno;
            flow._nPackets++;
            return p;
        }

        void free() {
            flow()._nPackets--;
            _packetdb.freePacket(this);
        }

        inline seq_t seqno() const {return _seqno;}
        inline simtime_picosec ts() const {return _ts;}
        inline void set_ts(simtime_picosec ts) {_ts = ts;}

    protected:
        seq_t _seqno;
        simtime_picosec _ts;

        static PacketDB<DataPacket> _packetdb;
};

class DataAck : public Packet
{
    public:
        typedef DataPacket::seq_t seq_t;

        virtual ~DataAck(){}

        inline static DataAck* newpkt(PacketFlow &flow, route_t &route, seq_t seqno, seq_t ackno)
        {
            DataAck *p = _packetdb.allocPacket();
            p->set(flow, route, ACK_SIZE, ackno);
            p->_seqno = seqno;
            p->_ackno = ackno;
            flow._nPackets++;
            return p;
        }

        void free() {
            flow()._nPackets--;
            _packetdb.freePacket(this);
        }

        inline seq_t seqno() const {return _seqno;}
        inline seq_t ackno() const {return _ackno;}
        inline simtime_picosec ts() const {return _ts;}
        inline void set_ts(simtime_picosec ts) {_ts = ts;}

    protected:
        seq_t _seqno;
        seq_t _ackno;
        simtime_picosec _ts;

        static PacketDB<DataAck> _packetdb;
};

#endif /* DATAPACKET_H */
