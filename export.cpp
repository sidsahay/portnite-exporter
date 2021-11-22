#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Skeletal.hpp"

// TIL this works in the opposite order
glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4 &from) {
    glm::mat4 to;
    //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
    to[0][0] = from.a1;
    to[1][0] = from.a2;
    to[2][0] = from.a3;
    to[3][0] = from.a4;
    to[0][1] = from.b1;
    to[1][1] = from.b2;
    to[2][1] = from.b3;
    to[3][1] = from.b4;
    to[0][2] = from.c1;
    to[1][2] = from.c2;
    to[2][2] = from.c3;
    to[3][2] = from.c4;
    to[0][3] = from.d1;
    to[1][3] = from.d2;
    to[2][3] = from.d3;
    to[3][3] = from.d4;
    return to;
}

int main(int argc, char** argv) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(data_path("bastionik.dae"),
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType);

    if (scene == nullptr) {
        std::cerr << "Could not load asset.\n";
        return -1;
    }

    std::vector<std::string> level_order_node_names;
    std::vector<Node> nodes;

    auto find_idx = [](const std::vector<std::string>& vec, const std::string& e) -> int {
        auto it = std::find(vec.begin(), vec.end(), e);
        if (it != vec.end()) {
            return std::distance(vec.begin(), it);
        }
        else {
            return -1;
        }
    };

    std::deque<std::pair<const aiNode*, int>> worklist;
    worklist.push_back({scene->mRootNode, -1});
    while(!worklist.empty()) {
        const auto& p = worklist.front();
        level_order_node_names.push_back(std::string(p.first->mName.data));
        nodes.emplace_back(p.second, aiMatrix4x4ToGlm(p.first->mTransformation));
        for (auto child_idx = 0u; child_idx < p.first->mNumChildren; child_idx++) {
            worklist.push_back({p.first->mChildren[child_idx], level_order_node_names.size() - 1});
        }
        worklist.pop_front();
    }

    for (const auto& name : level_order_node_names) {
        auto idx = find_idx(level_order_node_names, name);
        std::cout << name << ", " << idx << ", " << nodes[idx].parent_id << std::endl;
    }


    auto num_animations = scene->mNumAnimations;
    std::vector<Animation> animations;

	for (auto anim_idx = 0u; anim_idx < num_animations; anim_idx++) {
		auto animation = scene->mAnimations[anim_idx];
		std::cout << animation->mName.data << ", " << animation->mNumChannels << std::endl;
		for (auto channel_idx = 0; channel_idx < animation->mNumChannels; channel_idx++) {
			auto node_anim = animation->mChannels[channel_idx];
			std::cout << "Found animation for " << node_anim->mNodeName.data << std::endl;
            auto node_idx = find_idx(level_order_node_names, std::string(node_anim->mNodeName.data));

			animations.emplace_back();
            auto& animation = animations.back();
            
            animation.num_frames = node_anim->mNumRotationKeys;
            animation.node_id = node_idx;
            nodes[node_idx].has_animation = true;
            nodes[node_idx].animation_id = animations.size() - 1;

            for (unsigned i = 0; i < node_anim->mNumRotationKeys; i++) {
                auto scale_aiv = node_anim->mScalingKeys[i].mValue;
                auto scale_mat = glm::scale(glm::mat4(1.f), glm::vec3(scale_aiv.x, scale_aiv.y, scale_aiv.z));

                auto translate_aiv = node_anim->mPositionKeys[i].mValue;
                auto translate_mat = glm::translate(glm::mat4(1.f),
                                                glm::vec3(translate_aiv.x, translate_aiv.y, translate_aiv.z));

                auto quat_aiq = node_anim->mRotationKeys[i].mValue;
                auto rotate_mat = glm::mat4_cast(glm::quat{quat_aiq.w, quat_aiq.x, quat_aiq.y, quat_aiq.z});

                // rebuild node transform from animation data
                // note to self: always scale then rotate then translate
                animation.keys[i] = translate_mat * rotate_mat * scale_mat;
            }

			std::cout << "Scaling keys: " << node_anim->mNumScalingKeys << std::endl;
			std::cout << "Position keys: " << node_anim->mNumPositionKeys << std::endl;
			std::cout << "Rptation keys: " << node_anim->mNumRotationKeys << std::endl;
		}
	}

    std::ofstream animations_out(data_path("skeletal/animations.dat"), std::ios::binary);
    write_chunk("anim", animations, &animations_out);
    animations_out.close();

    std::ofstream node_out(data_path("skeletal/nodes.dat"), std::ios::binary);
    write_chunk("node", nodes, &node_out);
    node_out.close();

    std::vector<int> num_meshes;
    num_meshes.push_back(scene->mNumMeshes);
    std::ofstream num_out(data_path("skeletal/num.dat"), std::ios::binary);
    write_chunk("nums", num_meshes, &num_out);
    num_out.close();

    for (auto mesh_idx = 0u; mesh_idx < scene->mNumMeshes; mesh_idx++) {
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<unsigned int> indices;
        std::vector<BoneWeight> bone_weights;
        std::vector<BoneID> bone_ids;
        std::vector<Bone> bones;

        std::string prefix = std::string("skeletal/mesh") + std::to_string(mesh_idx);

        const auto mesh = scene->mMeshes[mesh_idx];
        for (unsigned int vert_idx = 0; vert_idx < mesh->mNumVertices; vert_idx++) {
            vertices.push_back(mesh->mVertices[vert_idx].x);
            vertices.push_back(mesh->mVertices[vert_idx].y);
            vertices.push_back(mesh->mVertices[vert_idx].z);
            bone_weights.emplace_back();
            bone_ids.emplace_back();
        }

        std::ofstream vertices_out(data_path(prefix + std::string("vertices.dat")), std::ios::binary);
        write_chunk("vert", vertices, &vertices_out);
        vertices_out.close();

        for (unsigned int vert_idx = 0; vert_idx < mesh->mNumVertices; vert_idx++) {
            normals.push_back(mesh->mNormals[vert_idx].x);
            normals.push_back(mesh->mNormals[vert_idx].y);
            normals.push_back(mesh->mNormals[vert_idx].z);
        }

        std::ofstream normals_out(data_path(prefix + std::string("normals.dat")), std::ios::binary);
        write_chunk("norm", normals, &normals_out);
        normals_out.close();

        for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++) {
            const auto& face = mesh->mFaces[face_idx];
            for (unsigned int idx_idx = 0; idx_idx < face.mNumIndices; idx_idx++) {
                indices.push_back(face.mIndices[idx_idx]);
            }
        }

        std::ofstream indices_out(data_path(prefix + std::string("indices.dat")), std::ios::binary);
        write_chunk("indi", indices, &indices_out);
        indices_out.close();

        for (auto bone_idx = 0u; bone_idx < mesh->mNumBones; bone_idx++) {
            auto bone = mesh->mBones[bone_idx];
            auto node_idx = find_idx(level_order_node_names, std::string(bone->mName.data));
            bones.emplace_back(node_idx, aiMatrix4x4ToGlm(bone->mOffsetMatrix));
            std::cout << bone->mName.data << ", " << bone->mNumWeights << std::endl;

		    for (unsigned int weight_idx = 0; weight_idx < bone->mNumWeights; weight_idx++) {
                const auto& weight = bone->mWeights[weight_idx];
                bone_weights.at(weight.mVertexId).insert(weight.mWeight);
                bone_ids.at(weight.mVertexId).insert(bone_idx);
            }
	    }

        std::ofstream weights_out(data_path(prefix + std::string("weights.dat")), std::ios::binary);
        write_chunk("weig", bone_weights, &weights_out);
        weights_out.close();

        std::ofstream ids_out(data_path(prefix + std::string("ids.dat")), std::ios::binary);
        write_chunk("idss", bone_ids, &ids_out);
        ids_out.close();

        std::ofstream bones_out(data_path(prefix + std::string("bones.dat")), std::ios::binary);
        write_chunk("bone", bones, &bones_out);
        bones_out.close();



    }
}