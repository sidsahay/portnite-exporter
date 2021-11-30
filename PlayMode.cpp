
#include "PlayMode.hpp"
#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <map>
#include <deque>



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

std::ostream& operator<<(std::ostream& out, const BoneWeight& bw) {
	out << "[" << bw.weights[0] << ", " << bw.weights[1] << ", " << bw.weights[2] << ", " << bw.weights[3] << "]";
	return out;
}

//assert(sizeof(BoneWeight) == 16); // same as a vec4


std::ostream& operator<<(std::ostream& out, const BoneID& bid) {
	out << "[" << bid.ids[0] << ", " << bid.ids[1] << ", " << bid.ids[2] << ", " << bid.ids[3] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const glm::vec4& v) {
	out << "{" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "}";
	return out;
}

//assert(sizeof(BoneID) == 16); // same as a ivec4

struct BoneNode {
	int id;
	std::string name;
	
};

float tri[] = {
     0.5f,  0.5f, 0.0f,  // top right
     0.5f, -0.5f, 0.0f,  // bottom right
    -0.5f, -0.5f, 0.0f,  // bottom left
    -0.5f,  0.5f, 0.0f   // top left 
};  

unsigned int tri_ele[] = {0, 1, 3, 1, 2, 3};

const char* vertex_shader = "#version 330 core\n"
"layout (location = 0) in vec4 Position;\n"
"layout (location = 1) in ivec4 BoneIDs;\n"
"layout (location = 2) in vec4 BoneWeights;\n"
"layout (location = 3) in vec3 pass_Normal;\n"
"out vec3 Normal;\n"
"uniform mat4[64] BoneTransforms;\n"
"uniform mat4 MVP;\n"
"void main() {\n"
"	vec4 transformed = vec4(0, 0, 0, 1);\n"
"	for (int i = 0; i < 4; i++) {\n"
"		int index = BoneIDs[i];\n"
"		if (index != -1) transformed = transformed + BoneWeights[i] * BoneTransforms[index] * Position;\n"
"	}\n"
	"Normal = pass_Normal;\n"
"	gl_Position = MVP * transformed;\n"
"}\n";

const char* vertex_shader_pos = "#version 330 core\n"
"layout (location = 0) in vec4 Position;\n"
"uniform mat4 MVP;\n"
"void main() {\n"
"	gl_Position = MVP * transformed;\n"
"}\n";

const char* vertex_shader_line = "#version 330 core\n"
"layout (location = 0) in vec4 Position;\n"
"uniform mat4 MVP;\n"
"void main() {\n"
"	gl_Position = Position;\n"
"}\n";

const char* fragment_shader_line = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main() {\n"
"	FragColor = vec4(1, 1, 1, 1);\n"
"}\n";


const char* fragment_shader = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec3 Normal;\n"
"void main() {\n"
"	float c = abs(dot(Normal, normalize(vec3(1, 1, 0))));\n"
"	FragColor = vec4(c, c, c, 1);\n"
"}\n";

AnimatedMesh::AnimatedMesh(const aiMesh* m, const aiScene* s) : mesh(m), scene(s) {
	auto num_bones = mesh->mNumBones;
	std::cout << "Bone names: \n" << num_bones << std::endl;

	for (auto b = 0u; b < num_bones; b++) {
		std::cout << mesh->mBones[b]->mName.data << std::endl;
	}

	std::cout << "Animation names: \n";
	auto num_animations = scene->mNumAnimations;
	for (auto anim_idx = 0u; anim_idx < num_animations; anim_idx++) {
		auto animation = scene->mAnimations[anim_idx];
		std::cout << animation->mName.data << ", " << animation->mNumChannels << std::endl;
		for (auto channel_idx = 0; channel_idx < animation->mNumChannels; channel_idx++) {
			auto node_anim = animation->mChannels[channel_idx];
			std::cout << "Found animation for " << node_anim->mNodeName.data << std::endl;
			bone_name_to_animation[std::string(node_anim->mNodeName.data)] = node_anim;
			//num_animation_frames = node_anim->mNumPositionKeys;
			std::cout << "Scaling keys: " << node_anim->mNumScalingKeys << std::endl;
			std::cout << "Position keys: " << node_anim->mNumPositionKeys << std::endl;
			std::cout << "Rptation keys: " << node_anim->mNumRotationKeys << std::endl;
		}
	}

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &norm_vbo);
	glGenBuffers(1, &weight_vbo);
	glGenBuffers(1, &id_vbo);
	glGenBuffers(1, &ebo);

	for (unsigned int i = 0; i < mesh->mNumBones; i++) {
		bone_positions.emplace_back();
		bone_transforms.emplace_back();
	}

	for (unsigned int vert_idx = 0; vert_idx < mesh->mNumVertices; vert_idx++) {
		verts.push_back(mesh->mVertices[vert_idx].x);
		verts.push_back(mesh->mVertices[vert_idx].y);
		verts.push_back(mesh->mVertices[vert_idx].z);
		bone_weights.emplace_back();
		bone_ids.emplace_back();
	}
	
	for (auto bone_idx = 0u; bone_idx < num_bones; bone_idx++) {
		auto bone = mesh->mBones[bone_idx];
		std::cout << bone->mName.data << ", " << bone->mNumWeights << std::endl;
		bone_name_to_id[std::string(bone->mName.data)] = bone_idx;

		for (unsigned int weight_idx = 0; weight_idx < bone->mNumWeights; weight_idx++) {
			const auto& weight = bone->mWeights[weight_idx];
			bone_weights.at(weight.mVertexId).insert(weight.mWeight);
			bone_ids.at(weight.mVertexId).insert(bone_idx);
		}
	}

	for (unsigned int vert_idx = 0; vert_idx < mesh->mNumVertices; vert_idx++) {
		normals.push_back(mesh->mNormals[vert_idx].x);
		normals.push_back(mesh->mNormals[vert_idx].y);
		normals.push_back(mesh->mNormals[vert_idx].z);
	}

	for (unsigned int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++) {
		const auto& face = mesh->mFaces[face_idx];
		for (unsigned int idx_idx = 0; idx_idx < face.mNumIndices; idx_idx++) {
			indices.push_back(face.mIndices[idx_idx]);
		}
	}

	elements = indices.size();

	glBindVertexArray(vao);

	std::vector<float> v;

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, id_vbo);
	glBufferData(GL_ARRAY_BUFFER, bone_ids.size() * sizeof(BoneID), bone_ids.data(), GL_STATIC_DRAW);
	glVertexAttribIPointer(1, 4, GL_INT, 4 * sizeof(int), (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, weight_vbo);
	glBufferData(GL_ARRAY_BUFFER, bone_weights.size() * sizeof(BoneWeight), bone_weights.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, norm_vbo);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	glEnable(GL_DEPTH_TEST);

	// glBindVertexArray(0);

	// glBindBuffer(GL_ARRAY_BUFFER, 0);
	// glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void AnimatedMesh::update_bones(unsigned int current_animation_frame) {
	update_bones_rec(scene->mRootNode, glm::mat4(1.0f), current_animation_frame);
}

void AnimatedMesh::draw(unsigned int program) {
	glUseProgram(program);
	unsigned int bone_transforms_id = glGetUniformLocation(program, "BoneTransforms");
	glUniformMatrix4fv(bone_transforms_id, mesh->mNumBones, GL_FALSE, (const float*)bone_transforms.data());

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, elements, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	glUseProgram(0);
}

void AnimatedMesh::update_bones_rec(const aiNode* node, const glm::mat4& parent_transform, unsigned int current_animation_frame) {
	auto global_transform = parent_transform * aiMatrix4x4ToGlm(node->mTransformation);
	
	// if node has animation, calculate node transform from said animation
	const auto& found_anim = bone_name_to_animation.find(std::string(node->mName.data));
	if (found_anim != bone_name_to_animation.end()) {
		auto bone_scale_aiv = found_anim->second->mScalingKeys[current_animation_frame].mValue;
		auto bone_scale_mat = glm::scale(glm::mat4(1.f), glm::vec3(bone_scale_aiv.x, bone_scale_aiv.y, bone_scale_aiv.z));

		auto bone_translate_aiv = found_anim->second->mPositionKeys[current_animation_frame].mValue;
		auto bone_translate_mat = glm::translate(glm::mat4(1.f),
										glm::vec3(bone_translate_aiv.x, bone_translate_aiv.y, bone_translate_aiv.z));

		//std::cout << glm::vec4(bone_translate_aiv.x, bone_translate_aiv.y, bone_translate_aiv.z, 1.0f) << "\n";
		auto bone_quat_aiq = found_anim->second->mRotationKeys[current_animation_frame].mValue;
		auto bone_rotate_mat = glm::mat4_cast(glm::quat{bone_quat_aiq.w, bone_quat_aiq.x, bone_quat_aiq.y, bone_quat_aiq.z});

		// rebuild node transform from animation data
		// note to self: always rotate first then translate
		global_transform = parent_transform * bone_translate_mat * bone_rotate_mat * bone_scale_mat;
	}
	
	// if node represents a bone, set bone data
	const auto& found = bone_name_to_id.find(std::string(node->mName.data));
	if (found != bone_name_to_id.end()) {
		glm::mat4 final_transform = aiMatrix4x4ToGlm(scene->mRootNode->mTransformation) *
		global_transform *
		aiMatrix4x4ToGlm(mesh->mBones[found->second]->mOffsetMatrix);
		//std::cout << "Processing bone: " << node->mName.data << " id: " << found->second << std::endl;
		bone_transforms.at(found->second) = final_transform;
		bone_positions.at(found->second) = global_transform * glm::vec4(0, 0, 0, 1);
	}

	// recurse to children
	for (auto child_idx = 0u; child_idx < node->mNumChildren; child_idx++) {
		update_bones_rec(node->mChildren[child_idx], global_transform, current_animation_frame);
	}
};

PlayMode::PlayMode() {

	
    scene = importer.ReadFile(data_path("bastionik.dae"),
        aiProcess_CalcTangentSpace       |
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType);

    if (scene == nullptr) {
        std::cerr << "Could not load asset.\n";
		return;
    }
    
	std::function<void(const aiNode*)> visit_node;
	visit_node = [&](const aiNode* node) {
		std::cout << node->mName.data << ", " << node->mNumMeshes << std::endl;
		for (unsigned int child_idx = 0; child_idx < node->mNumChildren; child_idx++) {
			visit_node(node->mChildren[child_idx]);
		}
	};

	visit_node(scene->mRootNode);

	
	for (unsigned m = 0; m < scene->mNumMeshes; m++) {
		animated_meshes.emplace_back(scene->mMeshes[m], scene);
	}
	// animated_meshes.emplace_back(scene->mMeshes[0], scene);


	vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, 1, &vertex_shader, NULL);
	glCompileShader(vshader);

	fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, 1, &fragment_shader, NULL);
	glCompileShader(fshader);

	program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);

	
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)1280/(float)720, 0.1f, 100.0f);
	glm::mat4 view;
	eye = glm::vec3(-0.25f, -1.5, -1);
	focus = eye + glm::vec3(0, 1, 0.1);
	view = glm::lookAt(eye, 
  		   focus, 
  		   glm::vec3(0.0f, 0, -1.0f));

	glm::mat4 mvp = proj * view;
	
	glUseProgram(program);
	unsigned int mvp_id = glGetUniformLocation(program, "MVP");
	glUniformMatrix4fv(mvp_id, 1, GL_FALSE, (const float*)&mvp);
	glUseProgram(0);

	num_animation_frames = 180;
	current_animation_frame = 0;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	
	return false;
}

void PlayMode::update(float elapsed) {
	elapsed_time += elapsed;
	if (elapsed_time >= 0.033f) {
		elapsed_time = 0;
		update_bones = true;
	}
	
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (update_bones) {
		update_bones = false;
		current_animation_frame = (current_animation_frame + 1) % num_animation_frames;

		for (auto& animated_mesh : animated_meshes) {
			animated_mesh.update_bones(current_animation_frame);
		}
	}

	for (auto& animated_mesh : animated_meshes) {
		animated_mesh.draw(program);
	}
}
