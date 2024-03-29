#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/obj.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least32_t;
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#define REG_CTRL 0x0000
#define REG_STATUS 0x0008
#define REG_EEPROM 0x0014
#define REG_CTRL_EXT 0x0018
#define REG_RCTRL 0x0100
#define REG_RXDESCLO 0x2800
#define REG_RXDESCHI 0x2804
#define REG_RXDESCLEN 0x2808
#define REG_RXDESCHEAD 0x2810
#define REG_RXDESCTAIL 0x2818

#define REG_ICR 0xc0
#define REG_ICS 0xc8
#define REG_IMS 0xd0
#define REG_IMC 0xd8
#define REG_IAM 0xe0
#define REG_IVAR 0xE4

#define REG_TCTRL 0x0400
#define REG_TXDESCLO 0x3800
#define REG_TXDESCHI 0x3804
#define REG_TXDESCLEN 0x3808
#define REG_TXDESCHEAD 0x3810
#define REG_TXDESCTAIL 0x3818

#define REG_RDTR 0x2820   // RX Delay Timer Register
#define REG_RXDCTL 0x3828 // RX Descriptor Control
#define REG_RADV 0x282C   // RX Int. Absolute Delay Timer
#define REG_RSRPD 0x2C00  // RX Small Packet Detect Interrupt

#define REG_RAL 0x5400
#define REG_RAH 0x5404

#define REG_TIPG 0x0410 // Transmit Inter Packet Gap

#define RCTL_EN (1 << 1)            // Receiver Enable
#define RCTL_SBP (1 << 2)           // Store Bad Packets
#define RCTL_UPE (1 << 3)           // Unicast Promiscuous Enabled
#define RCTL_MPE (1 << 4)           // Multicast Promiscuous Enabled
#define RCTL_LPE (1 << 5)           // Long Packet Reception Enable
#define RCTL_LBM_NONE (0 << 6)      // No Loopback
#define RCTL_LBM_PHY (3 << 6)       // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER (1 << 8) // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH (2 << 8)  // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36 (0 << 12)        // Multicast Offset - bits 47:36
#define RCTL_MO_35 (1 << 12)        // Multicast Offset - bits 46:35
#define RCTL_MO_34 (2 << 12)        // Multicast Offset - bits 45:34
#define RCTL_MO_32 (3 << 12)        // Multicast Offset - bits 43:32
#define RCTL_BAM (1 << 15)          // Broadcast Accept Mode
#define RCTL_VFE (1 << 18)          // VLAN Filter Enable
#define RCTL_CFIEN (1 << 19)        // Canonical Form Indicator Enable
#define RCTL_CFI (1 << 20)          // Canonical Form Indicator Bit Value
#define RCTL_DPF (1 << 22)          // Discard Pause Frames
#define RCTL_PMCF (1 << 23)         // Pass MAC Control Frames
#define RCTL_SECRC (1 << 26)        // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256 (3 << 16)
#define RCTL_BSIZE_512 (2 << 16)
#define RCTL_BSIZE_1024 (1 << 16)
#define RCTL_BSIZE_2048 (0 << 16)
#define RCTL_BSIZE_4096 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384 ((1 << 16) | (1 << 25))

// Transmit Command

#define CMD_EOP (1 << 0)  // End of Packet
#define CMD_IFCS (1 << 1) // Insert FCS
#define CMD_IC (1 << 2)   // Insert Checksum
#define CMD_RS (1 << 3)   // Report Status
#define CMD_RPS (1 << 4)  // Report Packet Sent
#define CMD_VLE (1 << 6)  // VLAN Packet Enable
#define CMD_IDE (1 << 7)  // Interrupt Delay Enable

#define STAT_DD 1 // descriptor done

#define CTRL_FD (1 << 0)      // full duplex
#define CTRL_ASDE (1 << 5)    // auto speed detect enable
#define CTRL_SLU (1 << 6)     // set link up
#define CTRL_RST (1 << 26)    // reset
#define CTRL_PHY_RST (1 << 0) // PHY reset

#define ECTRL_DRV_LOAD (1 << 28) // driver loaded
#define ECTRL_IAME (1 << 24)     // int ack auto-mask enable

// TCTL Register

#define TCTL_EN (1 << 1)      // Transmit Enable
#define TCTL_PSP (1 << 3)     // Pad Short Packets
#define TCTL_CT_SHIFT 4       // Collision Threshold
#define TCTL_COLD_SHIFT 12    // Collision Distance
#define TCTL_SWXOFF (1 << 22) // Software XOFF Transmission
#define TCTL_RTLC (1 << 24)   // Re-transmit on Late Collision

#define TSTA_DD (1 << 0) // Descriptor Done
#define TSTA_EC (1 << 1) // Excess Collisions
#define TSTA_LC (1 << 2) // Late Collision
#define LSTA_TU (1 << 3) // Transmit Underrun

// ICR
#define ICR_LSC (1 << 2)   // link status change
#define ICR_RXO (1 << 6)   // recv overrun
#define ICR_RxQ0 (1 << 20) // recv q 0
#define ICR_RxQ1 (1 << 21) // recv q 1
#define ICR_TxQ0 (1 << 22) // send q 0
#define ICR_TxQ1 (1 << 23) // send q 1

#define IVAR_EN_RxQ0 (1 << 3)
#define IVAR_EN_RxQ1 (1 << 7)
#define IVAR_EN_TxQ0 (1 << 11)
#define IVAR_EN_TxQ1 (1 << 15)
#define IVAR_EN_OTHER (1 << 19)

struct e1000_rx_desc {
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint16_t checksum;
	volatile uint8_t status;
	volatile uint8_t errors;
	volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint8_t cso;
	volatile uint8_t cmd;
	volatile uint8_t status;
	volatile uint8_t css;
	volatile uint16_t special;
} __attribute__((packed));

#include <mutex>
#include <vector>

#include <twz/sys/dev/queue.h>

class tx_request
{
  public:
	struct packet_queue_entry packet;
};

class packet
{
  public:
	void *vaddr;
	size_t length;
	uint64_t pinaddr;
	bool mapped;
	bool cached;
};

#include <unordered_map>

class e1000_controller
{
  public:
	twzobj ctrl_obj, buf_obj, txqueue_obj, rxqueue_obj, info_obj, packet_obj;
	uint64_t buf_pin, packet_pin;
	size_t nr_tx_desc, nr_rx_desc;
	struct e1000_tx_desc *tx_ring;
	struct e1000_rx_desc *rx_ring;

	std::mutex mtx;
	std::vector<tx_request *> txs;
	std::vector<packet *> packet_buffers;
	std::unordered_map<uint32_t, packet *> packet_info_map;
	std::unordered_map<uint32_t, packet *> packet_desc_map;
	size_t packet_off = OBJ_NULLPAGE_SIZE;

	uint8_t mac[6];

	uint32_t cur_tx, head_tx, head_rx, tail_rx;
	bool init;
};
