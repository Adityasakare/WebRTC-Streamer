#ifndef CONFIG_H
#define CONFIG_H

// ====================== Server & Camera Settings ======================
#define DEFAULT_SERVER_URL      "http://127.0.0.1:57778"
#define DEFAULT_CONFIG_PATH     "cameras.conf"

#define STUN_SERVER "stun://stun.l.google.com:19302"

// ====================== Video Settings ======================
#define VIDEO_WIDTH         640
#define VIDEO_HEIGHT        480
#define VIDEO_FRAMERATE     30

// ====================== Recording Settings ======================
#define RECORDING_DIR           "recordings/"
#define RECORDING_CHUNKS_SECS   6

// ============== WebRTC stream encoding (low latency) ==============
#define H264_BITRATE_KBPS    1000   
#define H264_KEY_INT_MAX     30    
#define RTP_PAYLOAD_TYPE     96

// ============== Reconnect policy ==============
#define RECONNECT_DELAY_MS   2000
#define MAX_RECONNECT_TRIES  0   

#endif