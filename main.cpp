/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
#include "assets.gen.h"

#include <sifteo/menu.h>
using namespace Sifteo;
///////////////
static const unsigned numCubes = 3;
static const CubeSet allCubes(0, numCubes);
///////////////
Metadata M = Metadata()
    .title("Bluetooth Tai")
    .package("com.Joyscube.sdk.bluetooth", "1.0")
    .icon(Icon)
    .cubeRange(numCubes);

/*
 * When you allocate a BluetoothPipe you can optionally set the size of its transmit and receive queues.
 * We use a minimum-size transmit queue to keep latency low, since we're transmitting continuously.
 */
/** 
 * here changed the <1,8> to <1,1>, this will impact the number of packages 
 */
BluetoothPipe <1,1> btPipe;

// Bluetooth packet counters, available for debugging
BluetoothCounters btCounters;

///VideoBuffer vid;
static VideoBuffer vid[numCubes];
///For onAccelChange
static TiltShakeRecognizer motion[numCubes];

void onCubeTouch(void *, unsigned);
void onConnect();
void onDisconnect();
void onReadAvailable();
void onWriteAvailable();
void updatePacketCounts(int tx, int rx);
/**
* below added class SensorListener for neighbor 
*/
bool neighboring = false;
class SensorListener {
public:
    struct Counter {
//        unsigned touch;
        unsigned neighborAdd;
        unsigned neighborRemove;
    } counters[numCubes];

    void install()
    {
        Events::neighborAdd.set(&SensorListener::onNeighborAdd, this);
        Events::neighborRemove.set(&SensorListener::onNeighborRemove, this);
        Events::cubeAccelChange.set(&SensorListener::onAccelChange, this);
        Events::cubeBatteryLevelChange.set(&SensorListener::onBatteryChange, this);
        Events::cubeConnect.set(&SensorListener::onConnect, this);
		/**		
        Events::cubeTouch.set(&SensorListener::onTouch, this);
		*/
        // Handle already-connected cubes
        for (CubeID cube : CubeSet::connected())
            onConnect(cube);
		
    }

private:
    void onConnect(unsigned id)
    {
        CubeID cube(id);
        uint64_t hwid = cube.hwID();

        bzero(counters[id]);
        LOG("Cube %d connected\n", id);

        vid[id].initMode(BG0_ROM);
        vid[id].attach(id);
        motion[id].attach(id);

        // Draw the cube's identity
        String<128> str;
        str << "I am cube #" << cube << "\n";
        str << "hwid " << Hex(hwid >> 32) << "\n     " << Hex(hwid) << "\n\n";
        vid[cube].bg0rom.text(vec(1,2), str);

        // Draw initial state for all sensors
        onAccelChange(cube);
        onBatteryChange(cube);
//        onTouch(cube);
        drawNeighbors(cube);
    }
	
    void onBatteryChange(unsigned id)
    {
        CubeID cube(id);
        String<32> str;
        str << "bat:   " << FixedFP(cube.batteryLevel(), 1, 3) << "\n";
        vid[cube].bg0rom.text(vec(1,13), str);
    }
	
    void onNeighborRemove(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
        LOG("Neighbor Remove: %02x:%d - %02x:%d\n", firstID, firstSide, secondID, secondSide);
		neighboring = false;
        if (firstID < arraysize(counters)) {
            counters[firstID].neighborRemove++;
            drawNeighbors(firstID);
        }
        if (secondID < arraysize(counters)) {
            counters[secondID].neighborRemove++;
            drawNeighbors(secondID);
        }
    }

    void onNeighborAdd(unsigned firstID, unsigned firstSide, unsigned secondID, unsigned secondSide)
    {
        LOG("Neighbor Add: %02x:%d - %02x:%d\n", firstID, firstSide, secondID, secondSide);

		neighboring = true;
        if (firstID < arraysize(counters)) {
            counters[firstID].neighborAdd++;
            drawNeighbors(firstID);
        }
        if (secondID < arraysize(counters)) {
            counters[secondID].neighborAdd++;
            drawNeighbors(secondID);
        }
    }

    void drawNeighbors(CubeID cube)
    {
        Neighborhood nb(cube);

        String<64> str;
        str << "      "
            << Hex(nb.neighborAt(TOP), 2) << "\n   "
            << Hex(nb.neighborAt(LEFT), 2) << "    "
            << Hex(nb.neighborAt(RIGHT), 2) << "\n      "
            << Hex(nb.neighborAt(BOTTOM), 2) << "\n";

        str << "   +" << counters[cube].neighborAdd
            << ", -" << counters[cube].neighborRemove
            << "\n\n";

        BG0ROMDrawable &draw = vid[cube].bg0rom;
        draw.text(vec(1,6), str);

        drawSideIndicator(draw, nb, vec( 1,  0), vec(14,  1), TOP);
        drawSideIndicator(draw, nb, vec( 0,  1), vec( 1, 14), LEFT);
        drawSideIndicator(draw, nb, vec( 1, 15), vec(14,  1), BOTTOM);
        drawSideIndicator(draw, nb, vec(15,  1), vec( 1, 14), RIGHT);
    }

    static void drawSideIndicator(BG0ROMDrawable &draw, Neighborhood &nb,
        Int2 topLeft, Int2 size, Side s)
    {
        unsigned nbColor = draw.ORANGE;
        draw.fill(topLeft, size,
            nbColor | (nb.hasNeighborAt(s) ? draw.SOLID_FG : draw.SOLID_BG));
    }
	
	void onAccelChange(unsigned id)
	{
        CubeID cube(id);
        auto accel = cube.accel();

        String<64> str;
        str << "acc: "
            << Fixed(accel.x, 3)
            << Fixed(accel.y, 3)
            << Fixed(accel.z, 3) << "\n";

        unsigned changeFlags = motion[id].update();
        if (changeFlags) {
            // Tilt/shake changed

            LOG("Tilt/shake changed, flags=%08x\n", changeFlags);

            auto tilt = motion[id].tilt;
            str << "tilt:"
                << Fixed(tilt.x, 3)
                << Fixed(tilt.y, 3)
                << Fixed(tilt.z, 3) << "\n";

            str << "shake: " << motion[id].shake;
        }

        vid[cube].bg0rom.text(vec(1,10), str);	
	
	}
};

/**
* above added for neighbor 
*/
void main()
{
    /*
     * Display text in BG0_ROM mode on Cube 0
     */

    for (CubeID cube : allCubes) {
        vid[cube].initMode(BG0_ROM);
        vid[cube].attach(cube);

    }	
    /*
     * If Bluetooth isn't supported, don't go on.
     */

    if (!Bluetooth::isAvailable()) {
        vid[0].bg0rom.text(vec(1,2), "Not supported");
        vid[0].bg0rom.text(vec(1,3), "on this Base.");
        vid[0].bg0rom.plot(vec(7,5), vid[0].bg0rom.FROWN);
        while (1)
            System::paint();
    }

    // Zero out our counters
    btCounters.reset();

    /*
     * Advertise some "game state" to the peer. Mobile apps can read this
     * "advertisement" buffer without interrupting the game. This may have
     * information that's useful on its own, or it may be information that
     * tells a mobile app whether or not we're in a game state where Bluetooth
     * interaction makes sense.
     *
     * In this demo, we'll report the number of times the cube has been touched,
     * and its current touch state. Advertisement data is totally optional, and
     * if your game doesn't have a use for this feature it's fine not to use it.
     */
/** delete for bt test 20181102
    Events::cubeTouch.set(onCubeTouch);
    onCubeTouch(0, cube);
*/
    /*
     * Handle sending and receiving Bluetooth data entirely with Events.
     * Our BluetoothPipe is a buffer that holds packets that have been
     * received and are waiting to be processed, and packets we're waiting
     * for the system to send.
     *
     * To demonstrate the system's performance, we'll be trying to send
     * and receive packets as fast as possible. Every time there's buffer
     * space available in the transmit queue, we'll fill it with a packet.
     * Likewise, every received packet will be dealt with as soon as possible.
     *
     * When we transmit packets in this example, we'll fill them with our
     * cube's accelerometer and touch state. When we receive packets, they'll
     * be hex-dumped to the screen. We also keep counters that show how many
     * packets have been processed.
     *
     * If possible applications are encouraged to use event handlers so that
     * they only try to read packets when packets are available, and they only
     * write packets when buffer space is available. In this example, we always
     * want to read packets when they arrive, so we keep an onRead() handler
     * registered at all times. We also want to write as long as there's buffer
     * space, but only when a peer is connected. So we'll register and unregister
     * our onWrite() handler in onConnect() and onDisconnect(), respectively.
     *
     * Note that attach() will empty our transmit and receive queues. If we want
     * to enqueue write packets in onConnct(), we need to be sure the pipe is
     * attached before we set up onConnect/onDisconnect.
     */

	btPipe.attach();
//    Events::bluetoothReadAvailable.set(onReadAvailable);
	
    updatePacketCounts(0, 0);

    /*
     * Watch for incoming connections, and display some text on the screen to
     * indicate connection state.
     */
    Events::bluetoothConnect.set(onConnect);
    Events::bluetoothDisconnect.set(onDisconnect);

    if (Bluetooth::isConnected()) {
        onConnect();
    } else {
        onDisconnect();
    }

/**
* below added for neighbor 
*/	
    static SensorListener sensors;
    sensors.install();
/**
* above added for neighbor 
*/	
	
    /*
     * Everything else happens in event handlers, nothing to do in our main loop.
     */

    while (1) {

        for (unsigned n = 0; n < 60; n++) {
            System::paint();
        }
        /*
         * For debugging, periodically log the Bluetooth packet counters.
         */

        btCounters.capture();
        LOG("BT-Counters: rxPackets=%d txPackets=%d rxBytes=%d txBytes=%d rxUserDropped=%d\n",
            btCounters.receivedPackets(), btCounters.sentPackets(),
            btCounters.receivedBytes(), btCounters.sentBytes(),
            btCounters.userPacketsDropped());
    }
}

void onCubeTouch(void *context, unsigned id)
{
    /*
     * Keep track of how many times this cube has been touched
     */

    CubeID cube = id;
    bool isTouching = cube.isTouching();
    static uint32_t numTouches = 0;

    if (isTouching) {
        numTouches++;
    }

    /*
     * Set our current Bluetooth "advertisement" data to a blob of information about
     * cube touches. In a real game, you'd use this to store any game state that
     * a mobile app may passively want to know without explicitly requesting it
     * from the running game.
     *
     * This packet can be up to 19 bytes long. We'll use a maximum-length packet
     * as an example, but this packet can indeed be shorter.
     */

    struct {
        uint8_t count;
        uint8_t touch;
        uint8_t placeholder[17];
    } packet = {
        numTouches,
        isTouching
    };

    // Fill placeholder with some sequential bytes
    for (unsigned i = 0; i < sizeof packet.placeholder; ++i)
        packet.placeholder[i] = 'A' + i;

    LOG("Advertising state: %d bytes, %19h\n", sizeof packet, &packet);
    Bluetooth::advertiseState(packet);
}

void onConnect()
{
    LOG("onConnect() called\n");
    ASSERT(Bluetooth::isConnected() == true);

    vid[0].bg0rom.text(vec(0,2), "   Connected!   ");
//    vid[0].bg0rom.text(vec(0,3), "                ");
//    vid[0].bg0rom.text(vec(0,8), " Last received: ");

    // Start trying to write immediately

	if(btPipe.writeAvailable()){
		vid[0].bg0rom.text(vec(0,3), " writeAvailable ");
	}
	else{
		vid[0].bg0rom.text(vec(0,3), "   writeNG   ");
	}
	
    Events::bluetoothWriteAvailable.set(onWriteAvailable);
	onWriteAvailable();
}

void onDisconnect()
{
    LOG("onDisconnect() called\n");
    ASSERT(Bluetooth::isConnected() == false);

    vid[0].bg0rom.text(vec(0,2), " Waiting for a  ");
    vid[0].bg0rom.text(vec(0,3), " connection...  ");

    // Stop trying to write
    Events::bluetoothWriteAvailable.unset();
}

void updatePacketCounts(int tx, int rx)
{
    // Update and draw packet counters

    static int txCount = 0;
    static int rxCount = 0;

    txCount += tx;
    rxCount += rx;

    String<17> str;
/**    
	str << "RX: " << rxCount;
    vid[0].bg0rom.text(vec(1,6), str);
*/
    str.clear();
    str << "TX: " << txCount;
    vid[0].bg0rom.text(vec(1,5), str);
}

void packetHexDumpLine(const BluetoothPacket &packet, String<17> &str, unsigned index)
{
    str.clear();

    // Write up to 8 characters
    for (unsigned i = 0; i < 8; ++i, ++index) {
        if (index < packet.size()) {
            str << Hex(packet.bytes()[index], 2);
        } else {
            str << "  ";
        }
    }
}

void onReadAvailable()
{
    LOG("onReadAvailable() called\n");

    /*
     * This is one way to read packets from the BluetoothPipe; using read(),
     * and copying them into our own buffer. A faster but slightly more complex
     * method would use peek() to access the next packet, and pop() to remove it.
     */
    BluetoothPacket packet;
    while (btPipe.read(packet) && btPipe.readAvailable()) {		
//    while (btPipe.read(packet)) {	
        /*
         * We received a packet over the Bluetooth link!
         * Dump out its contents in hexadecimal, to the log and the display.
         */

        LOG("Received: %d bytes, type=%02x, data=%19h\n",
            packet.size(), packet.type(), packet.bytes());

        String<17> str;

        str << "len=" << Hex(packet.size(), 2) << " type=" << Hex(packet.type(), 2);
        vid[0].bg0rom.text(vec(1,10), str);

        packetHexDumpLine(packet, str, 0);
        vid[0].bg0rom.text(vec(0,12), str);

        packetHexDumpLine(packet, str, 8);
        vid[0].bg0rom.text(vec(0,13), str);

        packetHexDumpLine(packet, str, 16);
        vid[0].bg0rom.text(vec(0,14), str);

        // Update our counters
        updatePacketCounts(0, 1);
    }
}

/**
*	This is our map between Joyscube's MCC (motion collection controller) and Joystick ( 5 axis and 15 buttons).
*	You can modify the map accourding to your games' request. Here we use Tai as example.
*
*/
void onWriteAvailable()
{
    LOG("onWriteAvailable() called\n");

    /*
     * This is one way to write packets to the BluetoothPipe; using reserve()
     * and commit(). If you already have a buffer that you want to copy to the
     * BluetoothPipe, you can use write().
     */

    while (Bluetooth::isConnected() && btPipe.writeAvailable()) {
        /*
         * Access some buffer space for writing the next packet. This
         * is the zero-copy API for writing packets. Both reading and writing
         * have a traditional (one copy) API and a zero-copy API.
         */

        BluetoothPacket &packet = btPipe.sendQueue.reserve();

        /*
         * Fill most of the packet with dummy data
         */

        // 7-bit type code, for our own application's use
		// Do not change setType(0x00), internal use !!!
        packet.setType(0x00);		
        packet.resize(packet.capacity());

        for (unsigned i = 0; i < packet.capacity(); ++i) {
			packet.bytes()[i] = 0;			
        }
		
        /** 
         * Get the accelerometer data from Cube0 Cube1 Cube2...
		 * We have totally 20 bytes for HID transmit, first byte for padding ( system internal use )
		 * Second  packet.bytes()[0] byte for Joystick's axis.X
		 * Third   packet.bytes()[1] byte for Joystick's axis.Y
		 * Fourth  packet.bytes()[2] byte for Joystick's axis.Z
		 * Fifth   packet.bytes()[3] byte for Joystick's axis.Rx
		 * Then 15 bits for buttons
		 * Others  bits for reserved
         */	
		 
		Byte3 accel_Cube0 = vid[0].physicalAccel();
		Byte3 accel_Cube1 = vid[1].physicalAccel();
		Byte3 accel_Cube2 = vid[2].physicalAccel();
		CubeID cube0 = 0;		
		CubeID cube1 = 1; 
		CubeID cube2 = 2;
		bool isTouching_Cube0 = cube0.isTouching();		
		bool isTouching_Cube1 = cube1.isTouching();
		bool isTouching_Cube2 = cube2.isTouching();

		/**
		 * Prepare the package, cube0 for the axis.X & axis.Y axis.Z
		 * cube1 for the axis.Rx
		 * 20 or 40 is used to gain the axis.X & axis.Y between 0 to 107		 
		 * 107 and 20 need to be changed together due to our range for axis is 0~127
		 */	
		 
		// accel_Cube0.x => X	packet.bytes()[0]
		if (accel_Cube0.x < 87 && accel_Cube0.x > 0) {
			packet.bytes()[0] = accel_Cube0.x + 40;
		}
		else if (accel_Cube0.x > -87 && accel_Cube0.x < 0) {
			packet.bytes()[0] = accel_Cube0.x - 40;		
		}
		else {
			packet.bytes()[0] = accel_Cube0.x;			
		}
		
		// accel_Cube0.y => Y	packet.bytes()[1]		
		if (accel_Cube0.y < 107 && accel_Cube0.y > 0) {
			packet.bytes()[1] = accel_Cube0.y + 20;
		}
		else if (accel_Cube0.y > -107 && accel_Cube0.y < 0) {
			packet.bytes()[1] = accel_Cube0.y - 20;		
		}
		else {
			packet.bytes()[1] = accel_Cube0.y;			
		}
		
		// accel_Cube0.z => Z	packet.bytes()[2]		
		if (accel_Cube0.z < 107 && accel_Cube0.z > 0) {
			packet.bytes()[2] = accel_Cube0.z + 20;
		}
		else if (accel_Cube0.z > -107 && accel_Cube0.z < 0) {
			packet.bytes()[2] = accel_Cube0.z - 20;		
		}
		else {
			packet.bytes()[2] = accel_Cube0.z;			
		}
		
		// accel_Cube1.Z => Rx	packet.bytes()[3]		
		if (accel_Cube1.z < 107 && accel_Cube1.z > 0) {
			packet.bytes()[3] = accel_Cube1.z + 20;
		}
		else if (accel_Cube1.z > -107 && accel_Cube1.z < 0) {
			packet.bytes()[3] = accel_Cube1.z - 20;		
		}
		else {
			packet.bytes()[3] = accel_Cube1.z;			
		}
		
		/**
		 * Prepare the buttons, cube1&cube2 use the 15 bits equal 15 buttons
		 * packet.bytes()[4] buttons 1~8: 0x01:A	0x02:B  0x04:C		0x08:X		0x10:Y		0x20:Z 	0x40:L1	0x80:R1
		 * packet.bytes()[5] buttons 1~7: 0x01:L2	0x02:R2	0x04:Start	0x08:Select	0x10:Mode	0x20:T1	0x40:T2	
		 * we need map above 15 buttons to all motions we have: 
		 * 
		 */
		 
		/**
		 * Area	packet.bytes()[4]
		 * buttons 1~8
		 *  		 
		 */	
		int8_t Trigger = 30;		 
//*******************************************************//		 

		// accel_Cube1.y, 		
		vid[1].bg0rom.text(vec(7,14), "A");				//A				
		if (accel_Cube1.y > Trigger) {						//Down	(7,14)
			packet.bytes()[4] = packet.bytes()[4] | 0x01;	//---A---//
			vid[1].bg0rom.text(vec(7,14), "A", vid[1].bg0rom.WHITE_ON_TEAL);	//A
		}
		else if (accel_Cube1.y < -Trigger) {				//Up	(7,1)
			packet.bytes()[4] = packet.bytes()[4] | 0x04;	//---C---// 
			
		}
		// accel_Cube1.x, 
		vid[1].bg0rom.text(vec(14,7), "B");				//B	
		if (accel_Cube1.x > Trigger) {						//Right	(14,7)
			packet.bytes()[4] = packet.bytes()[4] | 0x02;	//---B---//
			vid[1].bg0rom.text(vec(14,7), "B", vid[1].bg0rom.WHITE_ON_TEAL);	//B		
		}
		else if (accel_Cube1.x < -Trigger) {				//Left	(1,7)
			packet.bytes()[4] = packet.bytes()[4] | 0x20;	//---Z---//
			
		}
		
//*******************************************************//

		//accel_Cube2.y, 
		vid[2].bg0rom.text(vec(7,14), "X");				//X
		if (accel_Cube2.y > Trigger) {						//Down	(7,14)
			packet.bytes()[4] = packet.bytes()[4] | 0x08;	//---X---//
			vid[2].bg0rom.text(vec(7,14), "X", vid[2].bg0rom.WHITE_ON_TEAL);	//X	
		}
		else if (accel_Cube2.y < -Trigger) {				//Up	(7,1)
//			packet.bytes()[4] = packet.bytes()[4] | 0x10;	//---Y---// 
			
		}
		
		//accel_Cube2.x, 		
		vid[2].bg0rom.text(vec(14,7), "Y");				//Y	
		if (accel_Cube2.x > Trigger) {						//Right	(14,7)
			packet.bytes()[4] = packet.bytes()[4] | 0x10;	//---Y---//
			vid[2].bg0rom.text(vec(14,7), "Y", vid[2].bg0rom.WHITE_ON_TEAL);	//Y			
		}			
		else if (accel_Cube2.x < -Trigger) {				//Left	(1,7)
//			packet.bytes()[4] = packet.bytes()[4] | 0x80;	//---R1---//
			
		}		
//*******************************************************//
		
		//neighboring_Cube_all 		
		if (neighboring) {
			packet.bytes()[4] = packet.bytes()[4] | 0x02;	//---B---//
			
		}
//*******************************************************//	
	
		//isTouching_Cube0&1&2, trigger 
		if (isTouching_Cube0) {
			packet.bytes()[4] = packet.bytes()[4] | 0x01;	//---A---//
			
		}
		if (isTouching_Cube1) {
			packet.bytes()[4] = packet.bytes()[4] | 0x40;	//---L1---//
			
		}
		if (isTouching_Cube2) {
			packet.bytes()[4] = packet.bytes()[4] | 0x80;	//---R1---//
			
		}

		/**
		 * Area	packet.bytes()[5]
		 * buttons 1~7
		 *  		 
		 */	
/*		
			packet.bytes()[5] | = 0x01;	//L2
		
			packet.bytes()[5] | = 0x02;	//R2

			packet.bytes()[5] | = 0x04;	//Start

			packet.bytes()[5] | = 0x08;	//Select

			packet.bytes()[5] | = 0x10;	//Mode

			packet.bytes()[5] | = 0x20;	//T1

			packet.bytes()[5] | = 0x40;	//T2
*/		
		
        /*
         * Log the packet for debugging, and commit it to the FIFO.
         * The system will asynchronously send it to our peer.
         */

        LOG("Sending: %d bytes, type=%02x, data=%19h\n",
            packet.size(), packet.type(), packet.bytes());

        btPipe.sendQueue.commit();
        updatePacketCounts(1, 0);
    }
}

