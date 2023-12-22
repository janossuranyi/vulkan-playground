#include "vk_resources.h"

namespace vks {
    VkCompareOp convertCompareOp(CompareOp op)
    {
        switch (op) {
        case CompareOp::None:
        case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
        case CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case CompareOp::GreaterThanOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::LessThanOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        default:
            assert(false);
        }        
    }
    VkPrimitiveTopology convertTopology(PrimitiveTopology x)
    {
        switch (x) {
            case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            default:
                assert(false);
        }
    }
    VkCullModeFlags convertCullMode(CullMode x)
    {
        switch (x) {
        case CullMode::None: return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_BIT | VK_CULL_MODE_BACK_BIT;
        default:
            assert(0);
        }
    }
    VkPolygonMode convertPolygonMode(PolygonMode x)
    {
        switch (x) {
        case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
        default:
            assert(0);
        }
    }
    VkFrontFace convertFrontFace(FrontFace x)
    {
        switch (x) {
        case FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
        case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            assert(0);
        }
    }
    VkDescriptorType convertDescriptorType(ResourceType x)
    {
        switch (x) {
        case ResourceType::CombinedImage: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ResourceType::SampledImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case ResourceType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ResourceType::StorageBufferDyn: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case ResourceType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ResourceType::UniformBufferDyn: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        default:
            assert(0);
        }
    }
    VkBlendOp convertBlendOp(BlendOp x)
    {
        switch (x) {
        case BlendOp::None:
        case BlendOp::Add: return VK_BLEND_OP_ADD;
        case BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
        default:
            assert(0);
        }
    }
    VkBlendFactor convertBlendFactor(BlendFactor x)
    {
        switch (x) {
        case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        default:
            assert(0);
        }
    }

    bool isDepthFormat(VkFormat x)
    {
        if (x == VK_FORMAT_D16_UNORM ||
            x == VK_FORMAT_D32_SFLOAT ||
            x == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            return true;
        }
        return false;
    }
    bool isStencilFormat(VkFormat x)
    {
        return x == VK_FORMAT_D24_UNORM_S8_UINT || x == VK_FORMAT_D16_UNORM_S8_UINT || x == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
}
