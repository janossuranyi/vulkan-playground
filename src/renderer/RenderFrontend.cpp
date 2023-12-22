#include "pch.h"
#include "renderer/RenderFrontend.h"
#include "jsrlib/jsr_semaphore.h"
#include "jsrlib/jsr_logger.h"
#include "jobsys.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace jsr {

    S_MATERIAL convertMaterialtoShaderFormat(const Material& m, const VulkanRenderer* backend);

    std::vector<glm::vec3> Model::getPositions(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        std::vector<glm::vec3> out;
        if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
            int positions = primitive.attributes.at("POSITION");
            int view = model.accessors[positions].bufferView;
            int buffer = model.bufferViews[view].buffer;

            glm::vec3* buf = (glm::vec3*)(model.buffers[buffer].data.data() + model.bufferViews[view].byteOffset + model.accessors[positions].byteOffset);
            out.resize(model.accessors[positions].count);
            memcpy(out.data(), buf, out.size() * sizeof(glm::vec3));
        }
        return out;
    }

    std::vector<glm::vec3> Model::getNormals(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        std::vector<glm::vec3> out;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            int accessor = primitive.attributes.at("NORMAL");
            int view = model.accessors[accessor].bufferView;
            int buffer = model.bufferViews[view].buffer;

            glm::vec3* buf = (glm::vec3*)(model.buffers[buffer].data.data() + model.bufferViews[view].byteOffset + model.accessors[accessor].byteOffset);
            out.resize(model.accessors[accessor].count);
            memcpy(out.data(), buf, out.size() * sizeof(glm::vec3));
        }
        return out;
    }

    std::vector<glm::vec4> Model::getTangents(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        std::vector<glm::vec4> out;
        if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
            int accessor = primitive.attributes.at("TANGENT");
            int view = model.accessors[accessor].bufferView;
            int buffer = model.bufferViews[view].buffer;

            glm::vec4* buf = (glm::vec4*)(model.buffers[buffer].data.data() + model.bufferViews[view].byteOffset + model.accessors[accessor].byteOffset);
            out.resize(model.accessors[accessor].count);
            memcpy(out.data(), buf, out.size() * sizeof(glm::vec4));
        }
        return out;
    }

    std::vector<glm::vec2> Model::getUVs(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        std::vector<glm::vec2> out;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            int accessor = primitive.attributes.at("TEXCOORD_0");
            int view = model.accessors[accessor].bufferView;
            int buffer = model.bufferViews[view].buffer;

            glm::vec2* buf = (glm::vec2*)(model.buffers[buffer].data.data() + model.bufferViews[view].byteOffset + model.accessors[accessor].byteOffset);
            out.resize(model.accessors[accessor].count);
            memcpy(out.data(), buf, out.size() * sizeof(glm::vec2));
        }
        return out;
    }

    std::vector<glm::vec4> Model::getColors(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        std::vector<glm::vec4> out;
        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end()) {
            int accessor = primitive.attributes.at("COLOR_0");
            int view = model.accessors[accessor].bufferView;
            int buffer = model.bufferViews[view].buffer;

            const void* buf = (model.buffers[buffer].data.data() + model.bufferViews[view].byteOffset + model.accessors[accessor].byteOffset);

            const auto& accessorRef = model.accessors[accessor];

            if (accessorRef.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
                return out;
            }

            out.resize(model.accessors[accessor].count);
            if (accessorRef.type == TINYGLTF_TYPE_VEC3) {
                const glm::vec3* ptr = reinterpret_cast<const glm::vec3*>(buf);
                for (int i = 0; i < out.size(); ++i) {
                    out[i] = glm::vec4(ptr[i], 1.0f);
                }
            }
            else {
                memcpy(out.data(), buf, out.size() * sizeof(glm::vec4));
            }
        }

        return out;
    }

    std::vector<int> Model::visibleNodes(const glm::mat4& mView, const glm::mat4& mProjection, NodeFilter nodeFilter, MaterialFilter materialFilter)
    {
        return std::vector<int>();
    }

    glm::mat4 Model::getWorldTransform(int object)
    {
        static const glm::mat4 mIDENT(1.0f);

        if (!m_nodes[object].dirty) {
            return m_nodes[object]._worldTransMatrix;
        }
        
        auto& obj = m_nodes[object];
        obj.dirty = false;

        glm::mat4 mParent = mIDENT;

        const int parent = obj.parent;

        if (parent != -1) {
            mParent = getWorldTransform(parent);
        }

        const glm::mat4 translate   = glm::translate(mIDENT, obj.position);
        const glm::mat4 rotate      = glm::mat4_cast(obj.rotation);
        const glm::mat4 scale       = glm::scale(mIDENT, obj.scale);
        auto mModel                 = (translate * rotate * scale);
        obj._worldTransMatrix       = mParent * mModel;

        return obj._worldTransMatrix;
    }


    bool Model::load_from_gltf(const std::filesystem::path& filename)
    {
        namespace fs = std::filesystem;
        std::string warn, err;
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        bool result = false;

        if (filename.extension() == ".gltf") {
            result = loader.LoadASCIIFromFile(&model, &err, &warn, filename.generic_u8string());
        }
        else {
            result = loader.LoadBinaryFromFile(&model, &err, &warn, filename.generic_u8string());
        }

        if (!result) {
            jsrlib::Error("Cannot load GLTF: %s", err.c_str());
            return false;
        }

        if (!warn.empty()) {
            jsrlib::Info("load GLTF warning: %s", warn.c_str());
        }

        m_images.resize(model.images.size());
        jsrlib::counting_semaphore imgCounter;
        std::mutex sync;
        for (int i = 0; i < model.images.size(); ++i)
        {
            jobsys.submitJob([this, i, filename, &model, &sync](int id)
                {
                    fs::path dds = filename.parent_path() / "dds" / (fs::path(model.images[i].uri).stem().string() + ".dds");
                    gli::texture loaded;
                    if (fs::exists(dds)) {
                        loaded = jsr::VulkanImage::loadImageHelper(dds, false);
                        jsrlib::Info("(%d / %d) %s loaded (threadID: %d)", model.images.size(), i, dds.filename().string().c_str(), id);
                    }
                    else {
                        loaded = jsr::VulkanImage::loadImageHelper(filename.parent_path() / model.images[i].uri,false);
                        jsrlib::Info("(%d / %d) %s loaded (threadID: %d)", model.images.size(), i, model.images[i].uri.c_str(), id);
                    }

                    std::unique_lock<std::mutex> lock(sync);
                    m_images[i] = m_backend->createImage(loaded, model.images[i].uri);

                }, &imgCounter);
        }
        imgCounter.wait();

        m_materials.resize(model.materials.size());
        m_shaderMaterials.resize(m_materials.size());

        for (int i = 0; i < model.materials.size(); ++i) {
            const auto& src = model.materials[i];

            if (src.alphaMode == "OPAQUE") m_materials[i].alphaMode = AlphaMode::ALPHA_MODE_OPAQUE;
            else if (src.alphaMode == "BLEND") m_materials[i].alphaMode = AlphaMode::ALPHA_MODE_BLEND;
            else if (src.alphaMode == "MASK") m_materials[i].alphaMode = AlphaMode::ALPHA_MODE_MASK;

            m_materials[i].type = MATERIAL_TYPE_METALLIC_ROUGHNESS;
            m_materials[i].baseColorFactor = glm::make_vec4((double*)src.pbrMetallicRoughness.baseColorFactor.data());
            m_materials[i].emissiveFactor = glm::make_vec3(src.emissiveFactor.data());
            m_materials[i].metallicFactor = (float)src.pbrMetallicRoughness.metallicFactor;
            m_materials[i].roughnessFactor = (float)src.pbrMetallicRoughness.roughnessFactor;
            m_materials[i].alphaCutoff = (float)src.alphaCutoff;
            m_materials[i].doubleSided = src.doubleSided;
            m_materials[i].normalScale = glm::vec3((float)src.normalTexture.scale);
            m_materials[i].normalScale.y *= -1.0f;
            m_materials[i].name = src.name;

            RenderPassResources shaderResources;
            const handle::Sampler defaultSampler = m_backend->getDefaultAnisotropeRepeatSampler();

            if (src.emissiveTexture.index > -1) {
                m_materials[i].emissiveTexture = m_images[model.textures[src.emissiveTexture.index].source];
            }
            if (src.normalTexture.index > -1) {
                m_materials[i].normalTexture = m_images[model.textures[src.normalTexture.index].source];
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_materials[i].normalTexture, defaultSampler, 1, 0));
            }
            else {
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_backend->getFlatDepthImage(), defaultSampler, 1, 0));
            }
            if (src.occlusionTexture.index > -1) {
                m_materials[i].occlusionTexture = m_images[model.textures[src.occlusionTexture.index].source];
            }
            if (src.pbrMetallicRoughness.baseColorTexture.index > -1) {
                m_materials[i].baseColorTexture = m_images[model.textures[src.pbrMetallicRoughness.baseColorTexture.index].source];
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_materials[i].baseColorTexture, defaultSampler, 0, 0));
            }
            else {
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_backend->getWhiteImage(), defaultSampler, 0, 0));
            }
            if (src.pbrMetallicRoughness.metallicRoughnessTexture.index > -1) {
                m_materials[i].specularTexture = m_images[model.textures[src.pbrMetallicRoughness.metallicRoughnessTexture.index].source];
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_materials[i].specularTexture, defaultSampler, 2, 0));
            }
            else {
                shaderResources.combinedImageSamplers.push_back(ImageResource(m_backend->getBlackImage(), defaultSampler, 2, 0));
            }
            m_shaderMaterials[i] = convertMaterialtoShaderFormat(m_materials[i], m_backend);

            //m_materials[i].shaderSet = m_backend->createDescriptorSet(m_pipeline, shaderResources);
        }

        StorageBufferDescription materialBufferDescr = {};
        materialBufferDescr.readOnly = true;
        materialBufferDescr.size = (uint32_t)(m_materials.size() * sizeof(S_MATERIAL));
        materialBufferDescr.initialData = m_shaderMaterials.data();

        m_materialBuffer = m_backend->createStorageBuffer(materialBufferDescr);

        int stat_vertexCount(0), stat_indexCount(0);
        {
            std::vector<jsr::Vertex> hwVertices;
            std::vector<uint16_t> hwIndices;
            hwVertices.reserve(1000000);
            hwIndices.reserve(1000000);

            uint32_t baseVertex = 0;
            uint32_t firstIndex = 0;

            MeshDescription meshInfo = {};
            jsr::IndexType idxType = INDEX_TYPE_UINT16;

            for (size_t meshIdx(0); meshIdx < model.meshes.size(); ++meshIdx)
            {
                const auto& mesh = model.meshes[meshIdx];
                FrontendMesh _mesh = {};
                _mesh.name = mesh.name;

                for (size_t primitiveIdx(0); primitiveIdx < mesh.primitives.size(); ++primitiveIdx)
                {
                    const auto& primitive = mesh.primitives[primitiveIdx];
                    const auto& indices = model.accessors[primitive.indices];
                    const uint16_t* index16 = (uint16_t*)(
                        model.buffers[model.bufferViews[indices.bufferView].buffer].data.data() +
                        model.bufferViews[indices.bufferView].byteOffset +
                        indices.byteOffset);

                    int positions = primitive.attributes.at("POSITION");
                    MeshPrimitive triangles = {};
                    auto xyz = getPositions(model, primitive);
                    auto normals = getNormals(model, primitive);
                    auto tangents = getTangents(model, primitive);
                    auto uvs = getUVs(model, primitive);

                    triangles.indexCount = (uint32_t)indices.count;
                    triangles.baseVertex = baseVertex;
                    triangles.firstIndex = firstIndex;
                    triangles.material = primitive.material;                
                    triangles.bbox[0] = glm::make_vec3(model.accessors[positions].minValues.data());
                    triangles.bbox[1] = glm::make_vec3(model.accessors[positions].maxValues.data());

                    baseVertex += (uint32_t)xyz.size();
                    firstIndex += (uint32_t)indices.count;

                    for (size_t vert(0); vert < xyz.size(); ++vert)
                    {
                        jsr::Vertex v{};
                        v.xyz = xyz[vert];
                        v.uv = uvs[vert];
                        v.pack_normal(normals[vert]);
                        v.pack_tangent(tangents[vert]);
                        v.pack_color(glm::vec4(1.0f));
                        hwVertices.push_back(v);
                    }
                    _mesh.primitives.push_back(triangles);

                    for (size_t x = 0; x < indices.count; ++x)
                    {
                        if (indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            hwIndices.push_back(index16[x]);
                        }
                        else {
                            idxType = INDEX_TYPE_UINT32;
                            hwIndices.push_back(index16[x * 2 + 0]);
                            hwIndices.push_back(index16[x * 2 + 1]);
                        }
                    }

                    /***************/
                    stat_vertexCount += static_cast<uint32_t>(xyz.size());
                    stat_indexCount += static_cast<uint32_t>(indices.count);
                    /***************/

                }
                m_meshes.push_back(_mesh);
            }

            meshInfo.indexBuffer = std::move(hwIndices);
            meshInfo.vertexBuffer = jsr::toCharArray(hwVertices.size() * sizeof(Vertex), hwVertices.data());
            meshInfo.indexCount = firstIndex;
            meshInfo.vertexCount = baseVertex;
            meshInfo.indexType = idxType;

            auto aMesh = m_backend->createMeshes({ meshInfo });
            assert(!aMesh.empty() && handle::is_valid(aMesh[0]));
            m_backendMesh = aMesh[0];

            jsrlib::Info("stat_vertexCount: %d, stat_indexCount: %d, total triangles: %d", stat_vertexCount, stat_indexCount, stat_indexCount/3);
        }

        const auto& scene = model.defaultScene > -1 ? model.scenes[model.defaultScene] : model.scenes.front();
        m_scene = scene.nodes;
        m_nodes.reserve(model.nodes.size());
        m_matrixes.reserve(model.nodes.size());

        for (size_t nodeIdx(0); nodeIdx < model.nodes.size(); ++nodeIdx)
        {
            const auto& node = model.nodes[nodeIdx];
            Node myNode;
            myNode.nodeType = nodeType_Empty;

            if (node.mesh > -1) {
                myNode.nodeType = nodeType_Mesh;
                myNode.object = node.mesh;
            }
            else if (node.camera > -1) {
                myNode.nodeType = nodeType_Camera;
                myNode.object = node.camera;
            }
            else if (node.extensions.find("KHR_lights_punctual") != node.extensions.end()) {
                myNode.nodeType = nodeType_Light;
                myNode.object = node.extensions.at("KHR_lights_punctual").Get("light").GetNumberAsInt();
            }
            
            myNode.children = node.children;
            myNode.parent = -1;

            if (node.matrix.empty()) {
                if (!node.translation.empty()) {
                    myNode.position = glm::make_vec3(node.translation.data());
                }
                if (!node.rotation.empty()) {
                    myNode.rotation = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
                }
                if (!node.scale.empty()) {
                    myNode.scale = glm::make_vec3(node.scale.data());
                }
            }
            else {
                auto mtx = glm::mat4(glm::make_mat4((double*)node.matrix.data()));
                glm::vec3 tmp0;
                glm::vec4 tmp1;
                bool b_decomposed = glm::decompose(mtx, myNode.scale, myNode.rotation, myNode.position, tmp0, tmp1);
                assert(b_decomposed);
            }

            myNode.dirty = true;
            m_nodes.push_back(myNode);
        }

        for (size_t nodeIdx(0); nodeIdx < model.nodes.size(); ++nodeIdx)
        {
            Node& myNode = m_nodes[nodeIdx];
            for (const auto& child : myNode.children) {
                m_nodes[child].parent = static_cast<int>(nodeIdx);
                m_nodes[child].dirty = true;
            }

            if (myNode.nodeType == nodeType_Mesh)
            {
                Bounds nodeBounds;
                const auto& mesh = m_meshes[myNode.object];
                for (const auto& it : mesh.primitives) {
                    nodeBounds.Extend(it.bbox[0]);
                    nodeBounds.Extend(it.bbox[1]);
                    m_bounds.Extend(nodeBounds.Transform(getWorldTransform(nodeIdx)));
                }
            }

        }

        for (size_t nodeIdx(0); nodeIdx < model.nodes.size(); ++nodeIdx)
        {
            S_MATRIXES matrixes;
            matrixes.mModel = this->getWorldTransform(static_cast<int32_t>(nodeIdx));
            matrixes.mNormal = glm::mat4(glm::transpose(glm::inverse(glm::mat3(matrixes.mModel))));            

            m_matrixes.push_back(matrixes);
        }

        StorageBufferDescription matrixBufferDescr = {};
        matrixBufferDescr.readOnly = true;
        matrixBufferDescr.size = (uint32_t)(m_matrixes.size() * sizeof(S_MATRIXES));
        matrixBufferDescr.initialData = m_matrixes.data();

        m_matrixBuffer = m_backend->createStorageBuffer(matrixBufferDescr);

        return result;
    }

    void Model::cleanup()
    {
        //m_backend->freeStorageBuffer(m_materialBuffer);
        for (auto& it : m_images) {
            //m_backend->freeImage(it);
        }
    }

    static S_MATERIAL convertMaterialtoShaderFormat(const Material& m, const VulkanRenderer* backend)
    {
        S_MATERIAL out = {};
        out.baseColorFactor = m.baseColorFactor;
        out.emissiveFactor = glm::vec4(m.emissiveFactor, 1.0f);
        out.flags = encodeAlphaMode(0, m.alphaMode);
        out.flags = encodeDoubleSided(out.flags, m.doubleSided);
        out.metallicFactor = m.metallicFactor;
        out.normalScale = glm::vec4(m.normalScale,1.0f);
        out.occlusionStrenght = m.occlusionStrenght;
        out.roughnessFactor = m.roughnessFactor;
        out.type = m.type;
        out.baseColorTexture = -1;
        out.emissiveTexture = -1;
        out.normalTexture = -1;
        out.specularTexture = -1;
        out.occlusionTexture = -1;

        if (m.baseColorTexture.index != jsr::handle::INVALID_INDEX) {
            out.baseColorTexture = backend->getImageGlobalIndex(m.baseColorTexture);
        }
        if (m.normalTexture.index != jsr::handle::INVALID_INDEX) {
            out.normalTexture = backend->getImageGlobalIndex(m.normalTexture);
        }
        if (m.specularTexture.index != jsr::handle::INVALID_INDEX) {
            out.specularTexture = backend->getImageGlobalIndex(m.specularTexture);
        }
        if (m.occlusionTexture.index != jsr::handle::INVALID_INDEX) {
            out.occlusionTexture = backend->getImageGlobalIndex(m.occlusionTexture);
        }
        if (m.emissiveTexture.index != jsr::handle::INVALID_INDEX) {
            out.emissiveTexture = backend->getImageGlobalIndex(m.emissiveTexture);
        }

        return out;
    }
}
