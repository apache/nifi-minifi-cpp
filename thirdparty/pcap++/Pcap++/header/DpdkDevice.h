#ifndef PCAPPP_DPDK_DEVICE
#define PCAPPP_DPDK_DEVICE

#include <pthread.h>
#include <MacAddress.h>
#include <SystemUtils.h>
#include <RawPacket.h>
#include <PcapLiveDevice.h>

/**
 * @file
 * This file and DpdkDeviceList.h provide PcapPlusPlus C++ wrapper for DPDK (stands for data-plan development kit). What is
 * DPDK? as quoting from http://dpdk.org: "DPDK is a set of libraries and drivers for fast packet processing... These libraries can be used to:
 * receive and send packets within the minimum number of CPU cycles (usually less than 80 cycles)... develop fast packet capture algorithms
 * (tcpdump-like)... run third-party fast path stacks... Some packet processing functions have been benchmarked up to hundreds million
 * frames per second, using 64-byte packets with a PCIe NIC"<BR>
 * As DPDK API is written in C, PcapPlusPlus wraps the main functionality in a C++ easy-to-use classes which should have minimum affect
 * on performance and packet processing rate. In addition it brings DPDK to the PcapPlusPlus framework and API so you can use DPDK
 * together with other PcapPlusPlus features such as packet parsing and editing, etc.<BR>
 * So how DPDK basically works? in order to boost packet processing performance on a commodity server DPDK is bypassing the Linux kernel.
 * All the packet processing activity happens in the user space so basically packets are delivered from NIC hardware queues directly
 * to user-space shared memory without going through the kernel. In addition DPDK uses polling instead of handling interrupts for each
 * arrived packet (as interupts create some delays). Other methods to boost packets processing implemented by DPDK are using Hugepages to
 * decrease the size of TLB that results in a much faster virtual to physical page conversion, thread affinity to bind threads to a
 * specific core, lock-free user-space multi-core synchronization using rings data structures and NUMA awareness to avoid expensive data
 * transfers between sockets.<BR>
 * Not every NIC supports kernel-bypass capabilities so DPDK cannot work with any NIC. The list of supported NICs are in DPDK's web-site
 * http://dpdk.org/doc/nics . For each such NIC the DPDK framework provides a module that called poll-mode-driver (PMD in short) that
 * enables this NIC to the working with DPDK. PcapPlusPlus wasn't tested with most PMDs but all of them should theoretically work as
 * PcapPlusPlus doesn't change the PMD behavior<BR>
 * DPDK has another basic data-structure called mbuf. An mbuf is DPDK wrapper struct for network packets. When working with packets
 * in DPDK you actually work with mbufs. The mbuf contains the packet data (obviously) but also some metadata on the packet such
 * as the DPDK port it was captured on, packet ref-count (which allows it to be referenced by several objects), etc. One important
 * concept is that DPDK doesn't allocate mbufs on-the-fly but uses mbuf pools. These pools is allocated on application startup and
 * used throughout the application. The goal of this, of course, is increasing packet processing performance as allocating memory has
 * its cost. So pool size is important and varies between applications. For example: an application that stores packets in memory
 * has to have a large pool of mbufs so mbufs doesn't run-out. PcapPlusPlus enables to choose the pool size at startup<BR>
 * <BR>
 * PcapPlusPlus main wrapper classes for DPDK are:
 *    - DpdkDevice - a class that wraps a DPDK port and provides all capabilities of receiving and sending packets to this port
 *    - DpdkDeviceList - a singleton class that initializes the DPDK infrastructure and creates DpdkDevice instances to all available ports.
 *      In addition it allows starting and stopping of worker threads
 *    - MBufRawPacket - a child class to RawPacket which customizes it for working with mbuf
 *    - In addition PcapPlusPlus provides a shell script to initialize DPDK prerequisites: setup-dpdk.sh. This is an easy-to-use script
 *      that sets up huge-pages, loads DPDK kernel module and sets up the NICs that will be used by DPDK. This script must run before an
 *      application that uses DPDK runs. If you forgot to run it the application will fail with an appropriate error that will remind
 * 
 * DPDK initialization using PcapPlusPlus:
 *    - Before application runs: run the setup-dpdk.sh script
 *    - On application startup call DpdkDeviceList#initDpdk() static method to initialize DPDK infrastructure and DpdkDevice instances
 *    - Open the relevant DpdkDevice(s)
 *    - Send & receive packets...
 */

struct rte_mbuf;
struct rte_mempool;
struct rte_eth_conf;

/**
* \namespace pcpp
* \brief The main namespace for the PcapPlusPlus lib
*/
namespace pcpp
{

	class DpdkDeviceList;
	class DpdkDevice;

	/**
	 * An enum describing all PMD (poll mode driver) types supported by DPDK. For more info about these PMDs please visit the DPDK web-site
	 */
	enum DpdkPMDType {
		/** Unknown PMD type */
		PMD_UNKNOWN,
		/** Link Bonding for 1GbE and 10GbE ports to allow the aggregation of multiple (slave) NICs into a single logical interface*/
		PMD_BOND,
		/** Intel E1000 PMD */
		PMD_E1000EM,
		/** Intel 1GbE PMD */
		PMD_IGB,
		/** Intel 1GbE virtual function PMD */
		PMD_IGBVF,
		/** Cisco enic (UCS Virtual Interface Card) PMD */
		PMD_ENIC,
		/** Intel fm10k PMD */
		PMD_FM10K,
		/** Intel 40GbE PMD */
		PMD_I40E,
		/** Intel 40GbE virtual function PMD */
		PMD_I40EVF,
		/** Intel 10GbE PMD */
		PMD_IXGBE,
		/** Intel 10GbE virtual function PMD */
		PMD_IXGBEVF,
		/** Mellanox ConnectX-3, ConnectX-3 Pro PMD */
		PMD_MLX4,
		/** Null PMD */
		PMD_NULL,
		/** pcap file PMD */
		PMD_PCAP,
		/** ring-based (memory) PMD */
		PMD_RING,
		/** VirtIO PMD */
		PMD_VIRTIO,
		/** VMWare VMXNET3 PMD */
		PMD_VMXNET3,
		/** Xen Project PMD */
		PMD_XENVIRT,
		/** AF_PACKET PMD */
		PMD_AF_PACKET
	};

	class DpdkDevice;

	/**
	 * @class MBufRawPacket
	 * A class that inherits RawPacket and wraps DPDK's mbuf object (see some info about mbuf in DpdkDevice.h) but is
	 * compatible with PcapPlusPlus framework. Using MBufRawPacket is be almost similar to using RawPacket, the implementation 
	 * differences are encapsulated in the class implementation. For example: user can create and manipulate a Packet object from 
	 * MBufRawPacket the same way it is done with RawPacket; User can use PcapFileWriterDevice to save MBufRawPacket to pcap the 
	 * same way it's used with RawPacket; etc.<BR>
	 * The main difference is that RawPacket contains a pointer to the data itself and MBufRawPacket is holding a pointer to an mbuf
	 * object which contains a pointer to the data. This implies that MBufRawPacket without an mbuf allocated to it is not usable.
	 * Getting instances of MBufRawPacket can be done in one to the following ways:
	 *    - Receiving packets from DpdkDevice. In this case DpdkDevice takes care of getting the mbuf from DPDK and wrapping it with
	 *      MBufRawPacket
	 *    - Creating MBufRawPacket from scratch (in order to send it with DpdkDevice, for example). In this case the user should call
	 *      the init() method after constructing the object in order to allocate a new mbuf from DPDK port pool (encapsulated by DpdkDevice)
	 * 
	 * Limitations of this class:
	 *    - Currently chained mbufs are not supported. An mbuf has the capability to be linked to another mbuf and create a linked list
	 *      of mbufs. This is good for Jumbo packets or other uses. MBufRawPacket doesn't support this capability so there is no way to
	 *      access the mbufs linked to the mbuf wrapped by MBufRawPacket instance. I hope I'll be able to add this support in the future
	 */
	class MBufRawPacket : public RawPacket
	{
		friend class DpdkDevice;

	private:
		struct rte_mbuf* m_MBuf;
		DpdkDevice* m_Device;

		void setMBuf(struct rte_mbuf* mBuf, timeval timestamp);
	public:

		/**
		 * A default c'tor for this class. Constructs an instance of this class without an mbuf attached to it. In order to allocate
		 * an mbuf the user should call the init() method. Without calling init() the instance of this class is not usable.
		 * This c'tor can be used for initializing an array of MBufRawPacket (which requires an empty c'tor)
		 */
		MBufRawPacket() : RawPacket(), m_MBuf(NULL), m_Device(NULL) { m_DeleteRawDataAtDestructor = false; }

		/**
		 * A d'tor for this class. Once called it frees the mbuf attached to it (returning it back to the mbuf pool it was allocated from)
		 */
		virtual ~MBufRawPacket();

		/**
		 * A copy c'tor for this class. The copy c'tor allocates a new mbuf from the same pool the original mbuf was
		 * allocated from, attaches the new mbuf to this instance of MBufRawPacket and copies the data from the original mbuf
		 * to the new mbuf
		 * @param[in] other The MBufRawPacket instance to copy from
		 */
		MBufRawPacket(const MBufRawPacket& other);

		/**
		 * Initialize an instance of this class. Initialization includes allocating an mbuf from the pool that resides in DpdkDevice.
		 * The user should call this method only once per instance. Calling it more than once will result with an error
		 * @param[in] device The DpdkDevice which has the pool to allocate the mbuf from
		 * @return True if initialization succeeded and false if this method was already called for this instance (and an mbuf is
		 * already attched) or if allocating an mbuf from the pool failed for some reason
		 */
		bool init(DpdkDevice* device);

		// overridden methods

		/**
		 * An assignment operator for this class. Copies the data from the mbuf attached to the other MBufRawPacket to the mbuf
		 * attached to this instance. If instance is not initialized (meaning no mbuf is attached) nothing will be copied and
		 * instance will remain uninitialized (also, an error will be printed)
		 * @param[in] other The MBufRawPacket to assign data from
		 */
		MBufRawPacket& operator=(const MBufRawPacket& other);

		/**
		 * Set raw data to the mbuf by copying the data to it. In order to stay compatible with the ancestor method
		 * which takes control of the data pointer and frees it when RawPacket is destroyed, this method frees this pointer right away after
		 * data is copied to the mbuf. So when using this method please notice that after it's called pRawData memory is free, don't
		 * use this pointer again. In addition, if raw packet isn't initialized (mbuf is NULL), this method will call the init() method
		 * @param[in] pRawData A pointer to the new raw data
		 * @param[in] rawDataLen The new raw data length in bytes
		 * @param[in] timestamp The timestamp packet was received by the NIC
		 * @return True if raw data was copied to the mbuf successfully, false if rawDataLen is larger than mbuf max size, if initialization
		 * failed or if copying the data to the mbuf failed. In all of these cases an error will be printed to log
		 */
		bool setRawData(const uint8_t* pRawData, int rawDataLen, timeval timestamp);

		/**
		 * Clears the object and frees the mbuf
		 */
		void clear();

		/**
		 * Append packet data at the end of current data. This method uses the same mbuf already allocated and tries to append more space and
		 * copy the data to it. If MBufRawPacket is not initialize (mbuf is NULL) or mbuf append failed an error is printed to log
		 * @param[in] dataToAppend A pointer to the data to append
		 * @param[in] dataToAppendLen Length in bytes of dataToAppend
		 */
		void appendData(const uint8_t* dataToAppend, size_t dataToAppendLen);

		/**
		 * Insert raw data at some index of the current data and shift the remaining data to the end. This method uses the
		 * same mbuf already allocated and tries to append more space to it. Then it just copies dataToAppend at the relevant index and shifts
		 * the remaining data to the end. If MBufRawPacket is not initialize (mbuf is NULL) or mbuf append failed an error is printed to log
		 * @param[in] atIndex The index to insert the new data to
		 * @param[in] dataToInsert A pointer to the new data to insert
		 * @param[in] dataToInsertLen Length in bytes of dataToInsert
		 */
		void insertData(int atIndex, const uint8_t* dataToInsert, size_t dataToInsertLen);

		/**
		 * Remove certain number of bytes from current raw data buffer. All data after the removed bytes will be shifted back. This method
		 * uses the mbuf already allocated and tries to trim space from it
		 * @param[in] atIndex The index to start removing bytes from
		 * @param[in] numOfBytesToRemove Number of bytes to remove
		 * @return True if all bytes were removed successfully, or false if MBufRawPacket is not initialize (mbuf is NULL), mbuf trim
		 * failed or logatIndex+numOfBytesToRemove is out-of-bounds of the raw data buffer. In all of these cases an error is printed to log
		 */
		bool removeData(int atIndex, size_t numOfBytesToRemove);

		/**
		 * This overridden method,in contrast to its ancestor RawPacket#reallocateData() doesn't need to do anything because mbuf is already
		 * allocated to its maximum extent. So it only performs a check to verify the size after re-allocation doesn't exceed mbuf max size
		 * @param[in] newBufferLength The new buffer length as required by the user
		 * @return True if new size is larger than current size but smaller than mbuf max size, false otherwise
		 */
		bool reallocateData(size_t newBufferLength);
	};

	/**
	 * @typedef OnDpdkPacketsArriveCallback
	 * A callback that is called when a burst of packets are captured by DpdkDevice
	 * @param[in] packets A pointer to an array of MBufRawPacket
	 * @param[in] numOfPackets The length of the array
	 * @param[in] threadId The thread/core ID who captured the packets
	 * @param[in] device A pointer to the DpdkDevice who captured the packets
	 * @param[in] userCookie The user cookie assigned by the user in DpdkDevice#startCaptureSingleThread() or DpdkDevice#startCaptureMultiThreads
	 */
	typedef void (*OnDpdkPacketsArriveCallback)(MBufRawPacket* packets, uint32_t numOfPackets, uint8_t threadId, DpdkDevice* device, void* userCookie);

	/**
	 * @class PciAddress
	 * A class representing a PCI address
	 */
	class PciAddress
	{
	public:
		/**
		 * Default c'tor that initializes all PCI address fields to 0 (until set otherwise, address will look like: 0000:00:00.0)
		 */
		PciAddress() { domain = 0; bus = 0; devid = 0; function = 0; }

		/**
		 * A c'tor that initializes all PCI address fields
		 * @param[in] domain Device domain
		 * @param[in] bus Device bus id
		 * @param[in] devid Device ID
		 * @param[in] function Device function
		 */
		PciAddress(uint16_t domain, uint8_t bus, uint8_t devid, uint8_t function)
		{
			this->domain = domain;
			this->bus = bus;
			this->devid = devid;
			this->function = function;
		}

		/** Device domain */
		uint16_t domain;
		/** Device bus id */
		uint8_t bus;
		/** Device ID */
		uint8_t devid;
		/** Device function */
		uint8_t function;

		/**
		 * @return The string format of the PCI address (xxxx:xx:xx.x)
		 */
		std::string toString()
		{
			char pciString[15];
			snprintf(pciString, 15, "%04x:%02x:%02x.%x", domain, bus, devid, function);
			return std::string(pciString);
		}

		/**
		 * Comparison operator overload. Two PCI addresses are equal if all of their address parts (domain, bus, devid, function) are equal
		 */
		bool operator==(const PciAddress &other) const
		{
			return (domain == other.domain && bus == other.bus && devid == other.devid && function == other.function);
		}
	};


	/**
	 * @class DpdkDevice
	 * Encapsulates a DPDK port and enables receiving and sending packets using DPDK as well as getting interface info & status, packet
	 * statistics, etc. This class has no public c'tor as it's constructed by DpdkDeviceList during initialization.<BR>
	 *
	 * __RX/TX queues__: modern NICs provide hardware load-balancing for packets. This means that each packet received by the NIC is hashed
	 * by one or more parameter (IP address, port, etc.) and goes into one of several RX queues provided by the NIC. This enables
	 * applications to work in a multi-core environment where each core can read packets from different RX queue(s). Same goes for TX
	 * queues: it's possible to write packets to different TX queues and the NIC is taking care of sending them to the network.
	 * Different NICs provide different number of RX and TX queues. DPDK supports this capability and enables the user to open the
	 * DPDK port (DpdkDevice) with a single or multiple RX and TX queues. When receiving packets the user can decide from which RX queue
	 * to read from, and when transmitting packets the user can decide to which TX queue to send them to. RX/TX queues are configured
	 * when opening the DpdkDevice (see openMultiQueues())<BR>
	 * 
	 * __Capturing packets__: there are two ways to capture packets using DpdkDevice:
	 *    - using worker threads (see DpdkDeviceList#startDpdkWorkerThreads() ). When using this method the worker should use the
	 *      DpdkDevice#receivePackets() methods to get packets from the DpdkDevice
	 *    - by setting a callback which is invoked each time a burst of packets arrives. For more details see 
	 *      DpdkDevice#startCaptureSingleThread()
	 *
	 * __Sending packets:__ DpdkDevice has various methods for sending packets. They enable sending raw packets, parsed packets, etc.
	 * for all opened TX queues. See DpdkDevice#sendPackets()<BR>
	 *
	 * __Get interface info__: DpdkDevice provides all kind of information on the interface/device such as MAC address, MTU, link status,
	 * PCI address, PMD (poll-mode-driver) used for this port, etc. In addition it provides RX/TX statistics when receiving or sending
	 * packets<BR>
	 *
	 * __Known limitations:__
	 *    - BPF filters are currently not supported by this device (as opposed to other PcapPlusPlus device types. This means that the 
	 *      device cannot filter packets before they get to the user
	 *    - It's not possible to set or change NIC load-balancing method. DPDK provides this capability but it's still not
	 *      supported by DpdkDevice
	 */
	class DpdkDevice : public IPcapDevice
	{
		friend class DpdkDeviceList;
		friend class MBufRawPacket;
	public:

		/**
		 * @struct DpdkDeviceConfiguration
		 * A struct that contains user configurable parameters for opening a DpdkDevice. All of these parameters have default values so 
		 * the user doesn't have to use these parameters or understand exactly what is their effect
		 */
		struct DpdkDeviceConfiguration
		{
			/**
			 * When configuring a DPDK RX queue, DPDK creates descriptors it will use for receiving packets from the network to this RX queue.
			 * This parameter enables to configure the number of descriptors that will be created for each RX queue
			 */
			uint16_t receiveDescriptorsNumber;

			/**
			 * When configuring a DPDK TX queue, DPDK creates descriptors it will use for transmitting packets to the network through this TX queue.
			 * This parameter enables to configure the number of descriptors that will be created for each TX queue
			 */
			uint16_t transmitDescriptorsNumber;

			/**
			 * A c'tor for this strcut
			 * @param[in] receiveDescriptorsNumber An optional parameter for defining the number of RX descriptors that will be allocated for each RX queue.
			 * Default value is 128
			 * @param[in] transmitDescriptorsNumber An optional parameter for defining the number of TX descriptors that will be allocated for each TX queue.
			 * Default value is 512
			 */
			DpdkDeviceConfiguration(uint16_t receiveDescriptorsNumber = 128, uint16_t transmitDescriptorsNumber = 512)
			{
				this->receiveDescriptorsNumber = receiveDescriptorsNumber;
				this->transmitDescriptorsNumber = transmitDescriptorsNumber;
			}
		};

		/**
		 * @struct LinkStatus
		 * A struct that contains the link status of a DpdkDevice (DPDK port). Returned from DpdkDevice#getLinkStatus()
		 */
		struct LinkStatus
		{
			/** Enum for describing link duplex */
			enum LinkDuplex 
			{
				/** Full duplex */
				FULL_DUPLEX, 
				/** Half duplex */
				HALF_DUPLEX 
			};

			/** True if link is up, false if it's down */
			bool linkUp;
			/** Link speed in Mbps (for example: 10Gbe will show 10000) */
			int linkSpeedMbps;
			/** Link duplex (half/full duplex) */
			LinkDuplex linkDuplex;
		};

		virtual ~DpdkDevice() {}

		/**
		 * @return The device ID (DPDK port ID)
		 */
		inline int getDeviceId() { return m_Id; }
		/**
		 * @return The device name which is in the format of 'DPDK_[PORT-ID]'
		 */
		inline std::string getDeviceName() { return std::string(m_DeviceName); }

		/**
		 * @return The MAC address of the device (DPDK port)
		 */
		inline MacAddress getMacAddress() { return m_MacAddress; }

		/**
		 * @return The name of the PMD (poll mode driver) DPDK is using for this device. You can read about PMDs in the DPDK documentation:
		 * http://dpdk.org/doc/guides/prog_guide/poll_mode_drv.html
		 */
		inline std::string getPMDName() { return m_PMDName; }

		/**
		 * @return The enum type of the PMD (poll mode driver) DPDK is using for this device. You can read about PMDs in the DPDK documentation:
		 * http://dpdk.org/doc/guides/prog_guide/poll_mode_drv.html
		 */
		inline DpdkPMDType getPMDType() { return m_PMDType; }

		/**
		 * @return The PCI address of the device
		 */
		inline PciAddress getPciAddress() { return m_PciAddress; }

		/**
		 * @return The device's maximum transmission unit (MTU) in bytes
		 */
		inline uint16_t getMtu() { return m_DeviceMtu; }

		/**
		 * Set a new maximum transmission unit (MTU) for this device
		 * @param[in] newMtu The new MTU in bytes
		 * @return True if MTU was set successfully, false if operation failed or if PMD doesn't support changing the MTU
		 */
		bool setMtu(uint16_t newMtu);

		/**
		 * @return True if this device is a virtual interface (such as VMXNET3, 1G/10G virtual function, etc.), false otherwise
		 */
		bool isVirtual();

		/**
		 * Get the link status (link up/down, link speed and link duplex)
		 * @param[out] linkStatus A reference to object the result shall be written to
		 */
		void getLinkStatus(LinkStatus& linkStatus);

		/**
		 * @return The core ID used in this context
		 */
		uint32_t getCurrentCoreId();

		/**
		 * @return The number of RX queues currently opened for this device (as configured in openMultiQueues() )
		 */
		uint16_t getNumOfOpenedRxQueues() { return m_NumOfRxQueuesOpened; }

		/**
		 * @return The number of TX queues currently opened for this device (as configured in openMultiQueues() )
		 */
		uint16_t getNumOfOpenedTxQueues() { return m_NumOfTxQueuesOpened; }

		/**
		 * @return The total number of RX queues available on this device
		 */
		uint16_t getTotalNumOfRxQueues() { return m_TotalAvailableRxQueues; }

		/**
		 * @return The total number of TX queues available on this device
		 */
		uint16_t getTotalNumOfTxQueues() { return m_TotalAvailableTxQueues; }


		/**
		 * Receive raw packets from the network
		 * @param[out] rawPacketsArr A vector where all received packets will be written into
		 * @param[in] rxQueueId The RX queue to receive packets from
		 * @return True if packets were received and no error occurred or false if device isn't opened, if device is currently capturing
		 * (using startCaptureSingleThread() or startCaptureMultiThreads() ), if rxQueueId doesn't exist on device, or if DPDK receive packets method returned
		 * an error
		 */
		bool receivePackets(RawPacketVector& rawPacketsArr, uint16_t rxQueueId);

		/**
		 * Receive raw packets from the network
		 * @param[out] rawPacketsArr A pointer to a non-allocated array of MBufRawPacket pointers where all received packets will be written into. The array
		 * will be allocated by this method and its length will be written into rawPacketArrLength. Notice it's the user responsibility to free the array and
		 * its content when done using it
		 * @param[out] rawPacketArrLength The length of MBufRawPacket pointers array will be written into
		 * @param[in] rxQueueId The RX queue to receive packets from
		 * @return True if packets were received and no error occurred or false if device isn't opened, if device is currently in capture mode
		 * (using startCaptureSingleThread() or startCaptureMultiThreads() ), if rxQueueId doesn't exist on device, or if DPDK receive packets method returned
		 * an error
		 */
		bool receivePackets(MBufRawPacket** rawPacketsArr, int& rawPacketArrLength, uint16_t rxQueueId);

		/**
		 * Receive parsed packets from the network
		 * @param[out] packetsArr A pointer to a non-allocated array of Packet pointers where all received packets will be written into. The array
		 * will be allocated by this method and its length will be written into packetsArrLength. Notice it's the user responsibility to free the array and
		 * its content when done using it
		 * @param[out] packetsArrLength The length of Packet pointers array will be written into
		 * @param[in] rxQueueId The RX queue to receive packets from
		 * @return True if packets were received and no error occurred or false if device isn't opened, if device is currently capturing
		 * (using startCaptureSingleThread() or startCaptureMultiThreads() ), if rxQueueId doesn't exist on device, or if DPDK receive packets method returned
		 * an error
		 */
		bool receivePackets(Packet** packetsArr, int& packetsArrLength, uint16_t rxQueueId);

		/**
		 * Send an array of raw packets to the network.<BR><BR>
		 * The sending algorithm works as follows: the algorithm tries to allocate a
		 * group of mbufs from the device mbuf pool. For each mbuf allocated a raw packet data is copied to the mbuf. This means that 
		 * the packets sent as input to this method aren't affected (aren't freed, changed, or anything like that). The algorithm will
		 * continue allocating mbufs until: no more raw packets to send; OR cannot allocate mbufs because mbug pool is empty; OR number
		 * of allocated mbufs is higher than a threshold of 80% of total TX descriptors. When one of these happen the algorithm will 
		 * try to send the mbufs it already got through DPDK API. DPDK will free these mbufs after sending them. Then the algorithm will 
		 * try to allocate mbufs again and send them again until no more raw packets are left to send to send or mbuf allocation failed 
		 * 3 times in a raw. Raw packets that are bigger than the size of an mbuf or with length 0 will be skipped. Same goes for raw 
		 * packets whose data could not be copied to the allocated mbuf for some reason. An appropriate error will be printed for 
		 * each such packet
		 * @param[in] rawPacketsArr A pointer to an array of raw packets
		 * @param[in] arrLength The length of the array
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packets will be sent to. The default is
		 * TX queue 0
		 * @return The number of packets successfully sent. If device is not opened or TX queue isn't open, 0 will be returned
		 */
		int sendPackets(const RawPacket* rawPacketsArr, int arrLength, uint16_t txQueueId = 0);

		/**
		 * Send an array of parsed packets to the network. For the send packets algorithm see sendPackets()
		 * @param[in] packetsArr A pointer to an array of parsed packet pointers
		 * @param[in] arrLength The length of the array
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packets will be sent to. The default is
		 * TX queue 0
		 * @return The number of packets successfully sent. If device is not opened or TX queue isn't open, 0 will be returned
		 */
		int sendPackets(const Packet** packetsArr, int arrLength, uint16_t txQueueId = 0);

		/**
		 * Send a vector of raw packets to the network. For the send packets algorithm see sendPackets()
		 * @param[in] rawPacketsVec The vector of raw packet
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packets will be sent to. The default is
		 * TX queue 0
		 * @return The number of packets successfully sent. If device is not opened or TX queue isn't open, 0 will be returned
		 */
		int sendPackets(const RawPacketVector& rawPacketsVec, uint16_t txQueueId = 0);

		/**
		 * Send packet raw data to the network. For the send packets algorithm see sendPackets(), but keep in mind this method sends
		 * only 1 packet
		 * @param[in] packetData The packet raw data to send
		 * @param[in] packetDataLength The length of the raw data
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packet will be sent to. The default is
		 * TX queue 0
		 * @return True if packet was sent successfully or false if device is not opened, TX queue isn't opened or the sending algorithm
		 * failed (for example: couldn't allocate an mbuf or DPDK returned an error)
		 */
		bool sendPacket(const uint8_t* packetData, int packetDataLength, uint16_t txQueueId = 0);

		/**
		 * Send a raw packet to the network. For the send packets algorithm see sendPackets(), but keep in mind this method sends
		 * only 1 packet
		 * @param[in] rawPacket The raw packet to send
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packet will be sent to. The default is
		 * TX queue 0
		 * @return True if packet was sent successfully or false if device is not opened, TX queue isn't opened or the sending algorithm
		 * failed (for example: couldn't allocate an mbuf or DPDK returned an error)
		 */
		bool sendPacket(const RawPacket& rawPacket, uint16_t txQueueId = 0);

		/**
		 * Send a parsed packet to the network. For the send packets algorithm see sendPackets(), but keep in mind this method sends
		 * only 1 packet
		 * @param[in] packet The parsed packet to send
		 * @param[in] txQueueId An optional parameter which indicates to which TX queue the packet will be sent on. The default is
		 * TX queue 0
		 * @return True if packet was sent successfully or false if device is not opened, TX queue isn't opened or the sending algorithm
		 * failed (for example: couldn't allocate an mbuf or DPDK returned an error)
		 */
		bool sendPacket(const Packet& packet, uint16_t txQueueId = 0);

		/**
		 * Overridden method from IPcapDevice. __BPF filters are currently not implemented for DpdkDevice__
		 * @return Always false with a "Filters aren't supported in DPDK device" error message
		 */
		bool setFilter(GeneralFilter& filter);

		/**
		 * Overridden method from IPcapDevice. __BPF filters are currently not implemented for DpdkDevice__
		 * @return Always false with a "Filters aren't supported in DPDK device" error message
		 */
		bool setFilter(std::string filterAsString);

		/**
		 * Open the DPDK device. Notice opening the device only makes it ready to use, it doesn't start packet capturing. This method initializes RX and TX queues,
		 * configures the DPDK port and starts it. Call close() to close the device. The device is opened in promiscuous mode
		 * @param[in] numOfRxQueuesToOpen Number of RX queues to setup. This number must be smaller or equal to the return value of getTotalNumOfRxQueues()
		 * @param[in] numOfTxQueuesToOpen Number of TX queues to setup. This number must be smaller or equal to the return value of getTotalNumOfTxQueues()
		 * @param[in] config Optional parameter for defining special port configuration parameters such as number of receive/transmit descriptors. If not set the default
		 * parameters will be set (see DpdkDeviceConfiguration)
		 * @return True if the device was opened successfully, false if device is already opened, if RX/TX queues configuration failed or of DPDK port
		 * configuration and startup failed
		 */
		bool openMultiQueues(uint16_t numOfRxQueuesToOpen, uint16_t numOfTxQueuesToOpen, const DpdkDeviceConfiguration& config = DpdkDeviceConfiguration());

		/**
		 * There are two ways to capture packets using DpdkDevice: one of them is using worker threads (see DpdkDeviceList#startDpdkWorkerThreads() ) and
		 * the other way is setting a callback which is invoked each time a burst of packets is captured. This method implements the second way.
		 * After invoking this method the DpdkDevice enters capture mode and starts capturing packets.
		 * This method assumes there is only 1 RX queue opened for this device, otherwise an error is returned. It then allocates a core and creates 1 thread
		 * that runs in an endless loop and tries to capture packets using DPDK. Each time a burst of packets is captured the user callback is invoked with the user
		 * cookie as a parameter. This loop continues until stopCapture() is called. Notice: since the callback is invoked for every packet burst
		 * using this method can be slower than using worker threads. On the other hand, it's a simpler way comparing to worker threads
		 * @param[in] onPacketsArrive The user callback which will be invoked each time a packet burst is captured by the device
		 * @param[in] onPacketsArriveUserCookie The user callback is invoked with this cookie as a parameter. It can be used to pass
		 * information from the user application to the callback
		 * @return True if capture thread started successfully or false if device is already in capture mode, number of opened RX queues isn't equal
		 * to 1, if the method couldn't find an available core to allocate for the capture thread, or if thread invocation failed. In
		 * all of these cases an appropriate error message will be printed
		 */
		bool startCaptureSingleThread(OnDpdkPacketsArriveCallback onPacketsArrive, void* onPacketsArriveUserCookie);

		/**
		 * This method does exactly what startCaptureSingleThread() does, but with more than one RX queue / capturing thread. It's called
		 * with a core mask as a parameter and creates a packet capture thread on every core. Each capturing thread is assigned with a specific
		 * RX queue. This method assumes all cores in the core-mask are available and there are enough opened RX queues to match for each thread.
		 * If these assumptions are not true an error is returned. After invoking all threads, all of them run in an endless loop
		 * and try to capture packets from their designated RX queues. Each time a burst of packets is captured the callback is invoked with the user
		 * cookie and the thread ID that captured the packets
		 * @param[in] onPacketsArrive The user callback which will be invoked each time a burst of packets is captured by the device
		 * @param[in] onPacketsArriveUserCookie The user callback is invoked with this cookie as a parameter. It can be used to pass
		 * information from the user application to the callback
		 * @param coreMask The core-mask for creating the cpature threads
		 * @return True if all capture threads started successfully or false if device is already in capture mode, not all cores in the core-mask are
		 * available to DPDK, there are not enough opened RX queues to match all cores in the core-mask, or if thread invocation failed. In
		 * all of these cases an appropriate error message will be printed
		 */
		bool startCaptureMultiThreads(OnDpdkPacketsArriveCallback onPacketsArrive, void* onPacketsArriveUserCookie, CoreMask coreMask);

		/**
		 * If device is in capture mode started by invoking startCaptureSingleThread() or startCaptureMultiThreads(), this method
		 * will stop all capturing threads and set the device to non-capturing mode
		 */
		void stopCapture();

		/**
		 * @return The number of free mbufs in device's mbufs pool
		 */
		int getAmountOfFreeMbufs();

		/**
		 * @return The number of mbufs currently in use in device's mbufs pool
		 */
		int getAmountOfMbufsInUse();

		//overridden methods

		/**
		 * Overridden method from IPcapDevice. It calls openMultiQueues() with 1 RX queue and 1 TX queue. 
		 * Notice opening the device only makes it ready to use, it doesn't start packet capturing. The device is opened in promiscuous mode
		 * @return True if the device was opened successfully, false if device is already opened, if RX/TX queues configuration failed or of DPDK port
		 * configuration and startup failed
		 */
		bool open() { return openMultiQueues(1, 1); };

		/**
		 * Close the DpdkDevice. When device is closed it's not possible work with it
		 */
		void close();

		/**
		 * Retrieve RX packet statistics from device
		 * @todo pcap_stat is a poor struct that doesn't contain all the information DPDK can provide. Consider using a more extensive struct
		 */
		void getStatistics(pcap_stat& stats);

	private:

		struct DpdkCoreConfiguration
		{
			int RxQueueId;
			bool IsCoreInUse;

			void clear() { RxQueueId = -1; IsCoreInUse = false; }

			DpdkCoreConfiguration() : RxQueueId(-1), IsCoreInUse(false) {}
		};

		DpdkDevice(int port, uint32_t mBufPoolSize);
		bool initMemPool(struct rte_mempool*& memPool, const char* mempoolName, uint32_t mBufPoolSize);

		bool configurePort(uint8_t numOfRxQueues, uint8_t numOfTxQueues);
		bool initQueues(uint8_t numOfRxQueuesToInit, uint8_t numOfTxQueuesToInit);
		bool startDevice();

		static int dpdkCaptureThreadMain(void *ptr);

		void clearCoreConfiguration();
		bool initCoreConfigurationByCoreMask(CoreMask coreMask);
		int getCoresInUseCount();

		void setDeviceInfo();

		typedef RawPacket* (*packetIterator)(void* packetStorage, int index);
		int sendPacketsInner(uint16_t txQueueId, void* packetStorage, packetIterator iter, int arrLength);

		char m_DeviceName[30];
		DpdkPMDType m_PMDType;
		std::string m_PMDName;
		PciAddress m_PciAddress;

		DpdkDeviceConfiguration m_Config;

		int m_Id;
		MacAddress m_MacAddress;
		uint16_t m_DeviceMtu;
		struct rte_mempool* m_MBufMempool;
		struct rte_mbuf* m_mBufArray[256];
		DpdkCoreConfiguration m_CoreConfiguration[MAX_NUM_OF_CORES];
		uint16_t m_TotalAvailableRxQueues;
		uint16_t m_TotalAvailableTxQueues;
		uint16_t m_NumOfRxQueuesOpened;
		uint16_t m_NumOfTxQueuesOpened;
		OnDpdkPacketsArriveCallback m_OnPacketsArriveCallback;
		void* m_OnPacketsArriveUserCookie;
		bool m_StopThread;

		bool m_WasOpened;

		 // RSS key used by the NIC for load balancing the packets between cores
		static uint8_t m_RSSKey[40];
	};

} // namespace pcpp

#endif /* PCAPPP_DPDK_DEVICE */
