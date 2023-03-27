/*
 * FIFO queue heder
 */
#ifndef QUEUE_H
#define QUEUE_H

#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include "leafswitch.h"

#include <list>

class Queue : public EventSource, public PacketSink
{
    public:
        enum QueueLocation { SRV_TOR, TOR_CORE, CORE_TOR, TOR_SRV };
        Queue(linkspeed_bps bitrate, mem_b maxsize, QueueLogger *logger);
        void doNextEvent();
        void handle_packet(Packet *pkt);
        void update_link_util(Packet *pkt);
        virtual void receivePacket(Packet &pkt);
        virtual void printStats();

        inline simtime_picosec drainTime(Packet *pkt) {
            return (simtime_picosec)(pkt->size()) * _ps_per_byte;
        }

        inline mem_b serviceCapacity(simtime_picosec t) {
            return (mem_b)(timeAsSec(t) * (double)_bitrate);
        }

        inline mem_b dctcpThreshold() {
            if (_bitrate <= 1000000000) { // 1gbps
                return 10 * MSS_BYTES;
            } else if (_bitrate <= 10000000000) { // 10gbps
                return 30 * MSS_BYTES;
            } else {
                return 90 * MSS_BYTES;
            }
        }

        mem_b _maxsize;   // Maximum queue size.
        mem_b _queuesize; // Current queue size.
        QueueLocation _location;
        Leafswitch *_leafswitch;
        double link_util;
        int64_t prev_time = 0;
        const double tau = 160; // usec
        const double alpha = 0.5; // TODO: Set this Knob
        const int64_t T_DRE = tau * alpha;

    protected:
        // Start serving the item at the head of the queue.
        virtual void beginService();

        // Wrap up serving the item at the head of the queue.
        virtual void completeService();

        // Apply ECN marking.
        void applyEcnMark(Packet &pkt);

        std::list<Packet*> _enqueued;  // List of packet enqueued.
        linkspeed_bps _bitrate;       // Speed at which queue drains.
        simtime_picosec _ps_per_byte; // Service time, in picosec per byte.

        // Housekeeping
        QueueLogger *_logger;
};

#endif /* QUEUE_H */
