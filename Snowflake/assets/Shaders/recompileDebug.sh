slangc TestShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o TestShader.spv
