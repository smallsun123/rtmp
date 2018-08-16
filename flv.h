#ifndef __FLV__H__
#define __FLV__H__


Data_Length(){
    DataOffset,     //(4byte) length of data prev
}

Flv_File_Header(){  // always 9 byte

    F,              // (1byte)
    L,              // (1byte)
    V,              // (1byte)

    Version,        // (1byte), File version (for example, 0x01 for FLV version 1)

    // +-+-+-+-+-+-+-+-+
    // |    R    |A|R|V|
    // +-+-+-+-+-+-+-+-+
    A_V_indicate(){ // (1byte)
        TypeFlagsReserved,      // (5bit), Shall be 0
        TypeFlagsAudio,         // (1bit), 1 = Audio tags are present
        TypeFlagsReserved,      // (1bit), Shall be 0
        TypeFlagsVideo,         // (1bit), 1 = Video tags are present
    }

    Data_Length()
}

Audio_Tag_Header(){
    
    indicate(){         // (1byte)
        /*
                Format of SoundData. The following values are defined:
                0 = Linear PCM, platform endian
                1 = ADPCM
                2 = MP3
                3 = Linear PCM, little endian
                4 = Nellymoser 16 kHz mono
                5 = Nellymoser 8 kHz mono
                6 = Nellymoser
                7 = G.711 A-law logarithmic PCM
                8 = G.711 mu-law logarithmic PCM
                9 = reserved
                10 = AAC
                11 = Speex
                14 = MP3 8 kHz
                15 = Device-specific sound
                Formats 7, 8, 14, and 15 are reserved.
                AAC is supported in Flash Player 9,0,115,0 and higher.
                Speex is supported in Flash Player 10 and higher.
            */
        SoundFormat,            // (4bit), 10 = AAC

        /*
                Sampling rate. The following values are defined:
                0 = 5.5 kHz
                1 = 11 kHz
                2 = 22 kHz
                3 = 44 kHz
            */
        SoundRate,              // (2bit), 

        /*
                Size of each audio sample. This parameter only pertains to
                uncompressed formats. Compressed formats always decode
                to 16 bits internally.
                0 = 8-bit samples
                1 = 16-bit samples
            */
        SoundSize,              // (1bit),

        /*
                Mono or stereo sound
                0 = Mono sound
                1 = Stereo sound
            */
        SoundType,              // (1bit),
    }

    if ( SoundFormat == 10 ) {  // AAC, more 1byte
    
        AACPacketType,          // (1byte),     0 = AAC sequence header, 1 = AAC raw

        if ( AACPacketType == 0 ) {
            AudioSpecificConfig,
        }

        if ( AACPacketType == 1 ) {
             Raw_AAC_frame_data,
        }
    }

    Raw_AAC_frame_data, 
}


Video_Tag_Header(){

    indicate(){     // (1byte), 

        /*
                Type of video frame. The following values are defined:
                1 = key frame (for AVC, a seekable frame)
                2 = inter frame (for AVC, a non-seekable frame)
                3 = disposable inter frame (H.263 only)
                4 = generated key frame (reserved for server use only)
                5 = video info/command frame
            */
        Frame_Type,         // (4bit)

        /*
                Codec Identifier. The following values are defined:
                2 = Sorenson H.263
                3 = Screen video
                4 = On2 VP6
                5 = On2 VP6 with alpha channel
                6 = Screen video version 2
                7 = AVC
            */
        CodecID,            // (4bit),  7 = AVC
    }

    if ( CodecID == 7 ) {   // AAC, more 4byte

        /*
                The following values are defined:
                0 = AVC sequence header, AVCDecoderConfigurationRecord
                1 = AVC NALU
                2 = AVC end of sequence (lower level NALU sequence ender is not required or supported)
            */
        AVCPacketType,      // (1byte), 

        /*
                if ( AVCPacketType == 1 ) {
                    CompositionTime = time offset;
                } else {
                    CompositionTime = 0;
                }
            */
        CompositionTime,    // (3byte), 
    }

    VideoData;
}

Flv_Tag_Header(){

    Type_indicate(){    // (1byte)
        Reserved,               // (2bit), Reserved for FMS, should be 0
        Filter,                 // (1bit),
        TagType,                // (5bit), 8 = audio, 9 = video, 0x12 = script data
    }

    DataSize,           // (3byte), Length of the message. Number of bytes after StreamID to end of tag (Equal to tag_length ®C 11)

    Timestamp,          // (3byte), ∫¡√Î

    TimestampExtended,  // (1byte), PTS = TimestampExtended<<24 | Timestamp.

    StreamID,           // (3byte), Always 0.


    Audio_Tag_Header(),
    Video_Tag_Header(),
    
    Data_Length()
}

#endif
