/*
 * api.h
 *
 *  Created on: Jul 7, 2010
 * 
 * Synopsys Inc.
 * SG DWC PT02
 */
/** 
 * @file
 * This is the upper layer of the HDMI TX API.
 * It is the entry poIM_INT32 and interface of the software package
 * It crosses information from the sink and information passed on from
 * the user to check if it violates the HDMI protocol or not.
 * It coordinates between the various modules of the API to program the 
 * controller successfully in the correct steps and in a compliant 
 * configuration.
 * Errors are written to the error buffer. use error_Get in error.h to read
 * last error.
 */

#ifndef _HDMI_API_H_
#define _HDMI_API_H_
#include <InfotmMedia.h>
#include "types.h"
#include "productParams.h"
#include "videoParams.h"
#include "audioParams.h"
#include "hdcpParams.h"
#include "dtd.h"
#include "shortVideoDesc.h"
#include "shortAudioDesc.h"
#include "hdmivsdb.h"
#include "monitorRangeLimits.h"
#include "videoCapabilityDataBlock.h"
#include "colorimetryDataBlock.h"
#include "speakerAllocationDataBlock.h"

/** event_t events to register a callback for in the API
 */
typedef enum
{
	EDID_READ_EVENT = 0, HPD_EVENT, DUMMY
} event_t;

typedef enum
{
	API_NOT_INIT = 1, API_INIT, API_HPD, API_EDID_READ, API_CONFIGURED
} state_t;
/**
 * Sets initial state of hardware and software to be able to be
 * configured
 * prepares to listen for HPD and to read the E-EDID structure at the sink.
 * @param address offset of the core instance from the bus base address
 * @param dataEnablePolarity data enable polarity
 * @param sfrClock external clock supplied to the HDMI core
 * @param force startup even without sensing HPD
 * @return TRUE when successful
 * @note to be either called upon HW bring up or solely after a standby
 */
IM_INT32 api_Initialize(IM_UINT16 address, IM_UINT8 dataEnablePolarity, IM_UINT16 sfrClock, IM_UINT8 force);
/**
 * Configure API.
 * Configure the modules of the API according to the parameters given by
 * the user. If EDID at sink is read, it does parameter checking using the 
 * Check methods against the sink's E-EDID. Violations are outputted to the 
 * buffer.
 * @param video parameters pointer
 * @param audio parameters pointer
 * @param product parameters pointer
 * @param hdcp parameters pointer
 * @return TRUE when successful
 * @note during this method, all controller's interrupts are disabled
 */
IM_INT32 api_Configure(videoParams_t * video, audioParams_t * audio,
		productParams_t * product, hdcpParams_t * hdcp);
IM_INT32 api_Audio_Cfg(videoParams_t * video, audioParams_t * audio,
		productParams_t * product, hdcpParams_t * hdcp);
/**
 * Prepare API modules and local variable to standby mode (and not respond
 * to interrupts)
 * @return TRUE when successful
 */
IM_INT32 api_Standby(void);
/**
 * sam : Prepare API modules and local variable to exit standby mode (and respond
 * to interrupts)
 * @return TRUE when successful
 */
IM_INT32 api_exitStandby(void);

IM_INT32 api_edid_done(void);
IM_INT32 api_DeInit(void);
IM_INT32 api_hpd_detect(IM_UINT16 address, void *handler);

/**
 * Read the contents of a certain byte in the hardware.
 * @param addr address of the byte to be read.
 * @return the value held in the byte.
 */
IM_UINT8 api_CoreRead(IM_UINT16 addr);
/**
 * Write a byte to a certain register in the hardware.
 * @note please use care when writing to the core as it
 * may alter the behaviour of the API
 * @param data byte to be wrtten to HW.
 * @param addr address of register to be written to
 */
void api_CoreWrite(IM_UINT8 data, IM_UINT16 addr);
/**
 * Event Handler determines the active HDMI event, and calls the associated
 *  function if event is enabled, assigned by the user. Event Handler shall
 *  be called explicitly by the user when the HDMICTRL interrupt line is active
 *  (or to be called periodically if polling detection is used instead).
 * @param param dummy argument pointer to void
 */
void api_EventHandler(void);
/**
 * Event Enable enables a certain event notification giving it a function
 * handle, in which this function is called whenever this event occurs.
 * @param idEvent the assigned ID of type EventType
 * @param handler of the function to be called when a certain even is
 * raised.
 * @param oneshot to indicate if it is to be enabled for only one time
 * @return TRUE when successful
 * @note this function must be called only before api initialization
 * that is either before any initialize calls are done or after
 * a standby. otherwise race conditions might occur as it uses strucutures
 * used within interrupts
 */
IM_INT32 api_EventEnable(event_t idEvent, handler_t handler, IM_UINT8 oneshot);
/**
 * Event Clear clears a certain event when the event is handled and responded
 *  to. To be called explicitly by the user when the event is handled and
 *  closed, to prevent same events of the same type, or line fluctuations to
 *  happen while user is evaluating and responding to the event.
 * @param idEvent the id given to the even.
 * @return TRUE when successful
 * @note this function must be called only before api initialization
 * that is either before any initialize calls are done or after
 * a standby. otherwise race conditions might occur as it uses strucutures
 * used within interrupts
 */
IM_INT32 api_EventClear(event_t idEvent);
/**
 * Disables a certain event  already assigned and enabled.  This will  prevent
 *  SW and HW from generating such event.
 * @param idEvent the assigned ID of type EventType
 * @return TRUE when successful
 * @note this function must be called only before api initialization
 * that is either before any initialize calls are done or after
 * a standby. otherwise race conditions might occur as it uses strucutures
 * used within interrupts
 */
IM_INT32 api_EventDisable(event_t idEvent);
/**
 * Trigger reading E-EDID sequence. It will enable edid interrupts.
 * @note make sure PHY is outputting TMDS clock (video) when this function
 * is invoked. some sinks disable E-EDID memory if no video is detected
 * @param handler of the function to be called when E-EDID reading is done
 * @return TRUE when successful
 */
IM_INT32 api_EdidRead(handler_t handler);
/**
 * Get number of  DTDs read from the sink's EDID structure
 * @return the number of Detailed Timing Descriptors read and parsed from EDID
 *  structure
 */
IM_INT32 api_EdidDtdCount(void);
/**
 * Get the indicated Detailed Timing Descriptor read from the sink's EDID structure
 * @param n the index of DTD as parsed from EDID structure
 * @param dtd pointer to dtd structure memory already allocated for the DTD
 * data to be copied to
 * @return FALSE when no valid DTD at that index is found
 */
 IM_INT32 api_EdidDtd(IM_UINT32 n, dtd_t * dtd);
/**
 * @param vsdb pointer to HDMI VSDB structure memory already allocated for the
 *  HDMI VSDB data to be copied to
 * @return FALSE when no valid HDMI VSDB has been parsed
 */
 IM_INT32 api_EdidHdmivsdb(hdmivsdb_t * vsdb);

/**
 * @param name pointer to IM_INT8 array memory already allocated for the monitor
 *  name to be copied to
 * @param length of the IM_INT8 array allocated
 * @return FALSE when no valid Monitor name is parsed
 */
 IM_INT32 api_EdidMonitorName(IM_INT8 * name , IM_UINT32 length);

/**
 * @param limits pointer to monitor range limits structure memory already 
 * allocated for the data to be copied to
 * @return FALSE when no valid Monitor Range Limits has been parsed
 */
 IM_INT32 api_EdidMonitorRangeLimits(monitorRangeLimits_t * limits);
 /**
  * Get the number of Short Video Descriptors, read from Video Data Block, at 
  * the sink's EDID data structure
  * @return the number of Short Video Descriptors read and parsed from EDID
  *  structure
 */
 IM_INT32 api_EdidSvdCount(void);
/**
 * Get an indicated Short Video Descriptors, read from Video Data Block, at the
 *  sink's EDID data structure
 * @param n the index of SVD as parsed from EDID structure
 * @param svd pointer to short video descriptor structure memory already
 *  allocated for the svd 
 * data to be copied to
 * @return TRUE when no valid SVD at that index is found
 */
 IM_INT32 api_EdidSvd(IM_UINT32 n, shortVideoDesc_t * svd);
 /**
  * Get the number of Short Audio Descriptors, read from Audio Data Block, at 
  * the sink's EDID data structure
  * @return the number of Short Audio Descriptors read and parsed from EDID structure
 */
 IM_INT32 api_EdidSadCount(void);
/**
 * Get an indicated Short Audio Descriptor, read from Audio Data Block, at the
 * sink's EDID data structure
 * @param n the index of Short Audio Descriptor as parsed from EDID structure
 * @param sad pointer to Short Audio Descriptor structure memory already 
 * allocated for the SAD data to be copied to
 * @return FALSE when no valid Short Audio Descriptor at that index is found
 */
 IM_INT32 api_EdidSad(IM_UINT32 n, shortAudioDesc_t * sad);
 /**
 * @param capability pointer to Video Capability Data Block structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Video Capability Data Block has been parsed
 */
 IM_INT32 api_EdidVideoCapabilityDataBlock(videoCapabilityDataBlock_t * capability);
 /**
 * @param allocation pointer to Speaker Allocation Data Block structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Speaker Allocation Data Block has been parsed
 */
 IM_INT32 api_EdidSpeakerAllocationDataBlock(speakerAllocationDataBlock_t * allocation);
 /**
 * @param colorimetry pointer to Colorimetry Data Block structure memory already allocated for the data to be copied to
 * @return FALSE when no valid Colorimetry Data Block has been parsed
 */
 IM_INT32 api_EdidColorimetryDataBlock(colorimetryDataBlock_t * colorimetry);
/**
* @return TRUE if Basic Audio is supported by sink
*/
 IM_INT32 api_EdidSupportsBasicAudio(void);
 /**
* @return TRUE if underscan is supported by sink
*/
 IM_INT32 api_EdidSupportsUnderscan(void);
/**
* @return TRUE if YCC:4:2:2 is supported by sink
*/
 IM_INT32 api_EdidSupportsYcc422(void);
/**
* @return TRUE if YCC:4:4:4 is supported by sink
*/
IM_INT32 api_EdidSupportsYcc444(void);
/**
 * Configure Audio Content Protection packets.
 * The function will configure ACP packets to be sent automatically with
 * the passed content and type until it is stopped using the respective
 * stop method.
 * @param type Content protection type (see HDMI1.3a Section 9.3)
 * @param fields ACP Type Dependent Fields
 * @param length of the ACP fields
 * @param autoSend Send Packets Automatically
 */
void api_PacketsAudioContentProtection(IM_UINT8 type, IM_UINT8 * fields, IM_UINT8 length,
		IM_UINT8 autoSend);
/**
 * Configure ISRC 1 & 2 Packets
 * The function will configure ISRC packets to be sent 
 * (automatically, when autoSend is set) with the passed content and type until
 *  it is stopped using the respective stop method.
 * The method will split the codes to ISRC1 and ISRC2 packets
 * automatically when needed. Only the valid packets will be sent (ie. if
 * ISRC2 has no valid data it will not be sent)
 * @param initStatus Initial status which the packets are sent with
 * (usually starting position)
 * @param codes ISRC codes as an array
 * @param length of the ISRC codes array
 * @param autoSend Send ISRC Automatically
 * @note Automatic sending does not change status automatically, it does
 * the insertion of the packets in the data islands.
 */
void api_PacketsIsrc(IM_UINT8 initStatus, IM_UINT8 * codes, IM_UINT8 length, IM_UINT8 autoSend);
/**
 * Set ISRC status that is changing during play back depending on position
 * (see HDMI 1.3a Section 8.8)
 * @param status the ISRC status code according to position of track
 */
void api_PacketsIsrcStatus(IM_UINT8 status);
/**
 * Configure Gamut Metadata packets to be sent with the passed content. It
 * is essential for the colorimetry to be set as Extended and the encoding
 * as YCbCr of the output video for the sink to interpret the Gamut Packet
 * content properly (and for the Gamut packets to be legal in the HDMI
 * protocol)
 * @param gbdContent Gamut Boundary Description Content as an array
 * @param length of the Gamut Boundary Description Content array
 */
void api_PacketsGamutMetadata(IM_UINT8 * gbdContent, IM_UINT8 length);
/**
 * Stop sending ACP packets when in auto send mode
 */
void api_PacketsStopSendAcp(void);
/**
 * Stop sending ISRC 1 & 2 packets when in auto send mode (ISRC 2 packets cannot be send without ISRC 1)
 */
void api_PacketsStopSendIsrc(void);
/**
 * Stop sending Source Product Description InfoFrame packets when in auto send mode
 */
void api_PacketsStopSendSpd(void);
/**
 * Stop sending Vendor Specific InfoFrame packets when in auto send mode
 */
void api_PacketsStopSendVsd(void);
/**
 * Send/stop sending AV Mute in the General Control Packet
 * @param enable TRUE set the AVMute in the general control packet
 */
void api_AvMute(IM_INT32 enable);
/**
 * Put audio samples to flat.
 * @param enable TRUE to mute audio
 */
void api_AudioMute(IM_INT32 enable);

/**
 * @return TRUE if HDCP is engaged
 */
IM_INT32 api_HdcpEngaged(void);
/**
 * The engine goes through the authentication
 * statesAs defined in the HDCP spec
 * @return a the state of the authentication machine
 * @note Used for debug purposes
 */
IM_UINT8 api_HdcpAuthenticationState(void);
/**
 * The engine goes through several cipher states
 * @return a the state of the cipher machine
 * @note Used for debug purposes
 */
IM_UINT8 api_HdcpCipherState(void);
/**
 * The engine goes through revocation states
 * @return a the state of the revocation the engine is going through
 * @note Used for debug purposes
 */
IM_UINT8 api_HdcpRevocationState(void);
/**
 * Bypass the HDCP encryption and transmit an unencoded content
 * (without ending authentication)
 * @param bypass Whether or not to bypass
 * @note Used for debug purposes
 */
IM_INT32 api_HdcpBypassEncryption(IM_INT32 bypass);
/**
 * To disable the HDCP encryption and transmit an
 * unencoded content and lose authentication
 * @param disable Whether or not HDCP is to be disabled
 * @note Used for debug purposes
 */
IM_INT32 api_HdcpDisableEncryption(IM_INT32 disable);

/**
 * Update HDCP SRM, list of maximum 5200 entries (version 1)
 * @param data array of the SRM file
 */
IM_INT32 api_HdcpSrmUpdate(const IM_UINT8 * data);

/**
 * Hot plug detect
 * @return TRUE when hot plug detected
 */
IM_INT32 api_HotPlugDetected(void);
/**
 * Dump core configuration to output stream
 * return error code
 */
IM_UINT8 api_ReadConfig(void);

#endif /* _HDMI_API_H_ */
