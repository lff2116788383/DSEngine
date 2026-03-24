{
  "targets": [
    {
      "target_name": "dsengine_bridge",
      "sources": [ 
        "src/bridge/dsengine_bridge.cpp",
        "../src/phase1/ecs/world.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../src",
        "../depends/entt-3.13.0/src",
        "../build/_deps/entt-src/single_include",
        "../depends",
        "../depends/miniaudio",
        "../depends/glm"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions", "-std=c++20" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "msvs_settings": {
        "VCCLCompilerTool": { 
          "ExceptionHandling": 1,
          "AdditionalOptions": [ "/std:c++20" ]
        }
      }
    }
  ]
}
