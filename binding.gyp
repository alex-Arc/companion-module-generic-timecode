{
    "targets": [
        {
            "target_name": "LtcDecoderRead",
            "sources": [ "src/NativeExtension.cc",
                        "src/libltc/src/ltc.h", "src/libltc/src/ltc.c",
                        "src/libltc/src/encoder.h", "src/libltc/src/encoder.c",
                        "src/libltc/src/decoder.h", "src/libltc/src/decoder.c",
                        "src/libltc/src/timecode.c", 
                        ],
            "include_dirs" : [
 	 			"<!(node -e \"require('nan')\")"
			]
        }
    ],
}