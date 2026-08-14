slangc DefaultShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o DefaultShader.spv
slangc CullingShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry cullMain -entry cmgMain -entry compactMain -o CullingShader.spv
slangc TestShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o TestShader.spv
slangc DebugAABBShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o DebugAABBShader.spv
slangc FlatShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o FlatShader.spv
slangc PickingShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o PickingShader.spv
slangc OutlineShader.slang -I . -target spirv -profile spirv_1_6 -emit-spirv-directly -g -O0 -fvk-use-scalar-layout -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o OutlineShader.spv
