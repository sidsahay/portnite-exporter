#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <map>

struct BoneWeight {
	float weights[4];

	BoneWeight() {
		weights[0] = 0.0f;
		weights[1] = 0.0f;
		weights[2] = 0.0f;
		weights[3] = 0.0f;
	}

	void insert(float weight) {
		for (int i = 0; i < 4; i++) {
			if (weights[i] == 0.0f) {
				weights[i] = weight;
				break;
			}
		}
	}
};


struct BoneID {
	int ids[4];

	BoneID() {
		ids[0] = -1;
		ids[1] = -1;
		ids[2] = -1;
		ids[3] = -1;
	}

	void insert(int id) {
		for (int i = 0; i < 4; i++) {
			if (ids[i] == -1) {
				ids[i] = id;
				break;
			}
		}
	}
};

struct AnimatedMesh {
	// all the rendering garbage
	unsigned int vao, vbo, norm_vbo, weight_vbo, id_vbo, ebo, elements;

	// bone information
	// this is duplicated rn. I know.
	std::map<std::string, unsigned int> bone_name_to_id;
	std::map<std::string, const aiNodeAnim*> bone_name_to_animation;
	std::vector<float> verts;
	std::vector<BoneWeight> bone_weights;
	std::vector<BoneID> bone_ids;
	std::vector<glm::vec4> bone_positions;
	std::vector<glm::mat4> bone_transforms;
	std::vector<float> normals;
	std::vector<unsigned int> indices;

	const aiMesh* mesh;
	const aiScene* scene;

	AnimatedMesh(const aiMesh* m, const aiScene* scene);

	void update_bones_rec(const aiNode* node, const glm::mat4& parent_transform, unsigned int current_animation_frame);
	void update_bones(unsigned int current_animation_frame);
	void draw(unsigned int program);
};

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	unsigned int vshader, fshader, program;
	unsigned int line_vshader, line_fshader, line_program, line_vbo, line_vao, line_ebo;
	Assimp::Importer importer;
	const aiScene* scene;
	const aiMesh* mesh;
	unsigned int current_animation_frame;
	unsigned int num_animation_frames;

	std::vector<AnimatedMesh> animated_meshes;

	glm::vec3 focus;
	glm::vec3 eye;

	bool update_bones = false;
	float elapsed_time = 0;
	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;



};
