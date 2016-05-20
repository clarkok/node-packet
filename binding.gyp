{
    "targets": [
        {
            "target_name" : "addon",
            "sources" : [ "src/addon.cpp" ],
            "cflags!" : [ "-fno-exceptions" ],
            "cflags_cc!" : [ "-fno-exceptions" ],
            "include_dirs" : [
                "<!(node -e \"require('nan')\")"
            ]
        }
    ]
}
