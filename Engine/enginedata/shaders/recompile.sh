slangc DefaultShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o DefaultShader.spv
slangc CullingShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry cullMain -entry cmgMain -entry compactMain -o CullingShader.spv
slangc TestShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o TestShader.spv
