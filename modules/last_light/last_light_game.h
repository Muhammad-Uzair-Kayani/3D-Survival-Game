#pragma once
#include "core/io/file_access.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "domain.h"
#include "hard_policy.h"
#include "neural_policy.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation/navigation_agent_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/spring_arm_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/label.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/packed_scene.h"
#include "strategy.h"
#include "world_contract.h"
#include <map>
#include <random>
#include <set>
#include <vector>
class LastLightGame : public Node3D {
	GDCLASS(LastLightGame, Node3D);
	struct Contact {
		Vector3 position, velocity;
		double seen = -100;
		float confidence = 0;
		bool hostile = false, visible = false;
		int health_band = 100;
	};
	struct Actor {
		std::map<int, Contact> contacts;
		std::map<int, double> failed_jobs;
		std::set<int> known_caches;
		struct CoverOption {
			int id;
			Vector3 position, peek;
			float score, route;
			bool firing;
		};
		std::vector<CoverOption> neural_cover_options;
		lastlight::TacticalDecision tactical_decision;
		lastlight::Tactic tactic = lastlight::Tactic::Progress;
		std::array<float, lastlight::TACTIC_COUNT> tactical_learning{};
		std::map<int, std::array<float, lastlight::TACTIC_COUNT>> contextual_values;
		int tactical_context = 0, learned_context = -1, learned_action = 0;
		double order_until = 0, next_investigate = 0;
		int order_peer = 0;
		double tactical_since = 0, tactical_until = 0, react_at = 0, last_damage_at = -100, last_noise_at = -100, next_tactical_scan = 0, next_pickup = 0;
		float suppression = 0, exposure_seconds = 0, footstep_distance = 0;
		int cover_id = -1, engagement_id = 0, role = 0, order = 0, task_claim = 0, seen_damage = 0, burst_shots = 0;
		double task_progress_at = 0, next_decision = 0;
		float task_distance = 10000;
		int support_id = 0;
		bool noise_danger = false;
		Vector3 aim_point;
		Vector3 last_noise, cover_goal, peek_goal, flank_goal, retreat_goal, order_position;
		String tactical_reason, order_reason;
		bool cover_manual = false, cover_protected = false, peek_clear = false, flank_clear = false;
		int bot_role = 0;
		lastlight::Personality personality;
		lastlight::StrategicPlan plan;
		std::vector<lastlight::StrategicPlan> alternatives;
		size_t plan_step = 0;
		double next_plan = 0, last_learning = 0, next_environment_use = 0, reposition_until = 0;
		Vector3 tactical_position;
		std::map<int, Vector3> known_items;
		std::map<int, String> known_item_kinds;
		std::set<int> known_empty_crates;
		std::map<int, double> profile_sessions;
		std::map<int, float> persistent_trust, persistent_fear;
		Array experience_log;
		int profile_revision = 0, previous_health = 100, previous_pickups = 0, previous_mission = 0;
		bool outcome_recorded = false;
		String environment_action;
		int environment_target = 0;
		float smooth_speed = 0, stance_weight = 0, aim_weight = 0, swim_weight = 0;
		bool was_grounded = true;
		Vector3 traversal_start, traversal_end;
		std::vector<Node3D *> gun_models;
		Node3D *held_item = nullptr, *vest_model = nullptr, *pack_model = nullptr;
		Label3D *name_tag = nullptr;
		float idle_clock = 0;
		int melee_combo = 0;
		int teammate_id = 0;
		double team_wait_until = 0;

		lastlight::Stats stats;
		CharacterBody3D *body = nullptr;
		Node3D *visual = nullptr, *left_leg = nullptr, *right_leg = nullptr, *left_arm = nullptr, *right_arm = nullptr;
		MeshInstance3D *pistol = nullptr, *sword = nullptr;
		NavigationAgent3D *nav = nullptr;
		AnimationPlayer *animation = nullptr;
		AnimationTree *animation_tree = nullptr;
		Skeleton3D *skeleton = nullptr;
		BoneAttachment3D *weapon_socket = nullptr;
		Node3D *pistol_model = nullptr, *sword_model = nullptr;
		CollisionShape3D *collider = nullptr;
		Ref<CapsuleShape3D> capsule;
		std::set<int> visible_actors, visible_items;
		Vector3 spawn_position, heard_position;
		double heard_until = 0, hit_feedback_time = -100;
		bool melee_pending = false;
		float travelled = 0, arrived_health = 0, arrived_time = 0;
		lastlight::Blend blend;
		std::map<int, lastlight::Belief> memory;
		Vector3 target, move;
		bool crouching = false, sprint = false, aiming = false, interacting = false, use_sword = false, jump = false;
		float yaw = 0, path_retry = 0, visual_yaw_offset = 0;
		String animation_state;
		String reason = "Spawned", intent = "Progress", last_clip;
	};
	struct Item {
		int id;
		String kind;
		Vector3 position;
		Node3D *visual = nullptr;
		bool available = true;
		int container = 0;
	};
	struct WorldObject {
		int id = 0;
		String kind, name;
		Vector3 position;
		Node3D *visual = nullptr;
		StaticBody3D *body = nullptr;
		bool used = false, open = false, locked = false;
		float value = 0;
	};
	struct Effect {
		String kind;
		Vector3 position;
		double until = 0;
		Node3D *visual = nullptr;
		int owner = 0;
	};
	std::vector<WorldObject> world_objects;
	std::vector<Effect> effects;
	std::vector<lastlight::StrategyNode> strategic_nodes;
	std::vector<lastlight::StrategyEdge> strategic_edges;
	struct CoverPoint {
		int id = 0;
		Vector3 position, normal, peek_a, peek_b;
		float height = 1;
		String label;
		int owner = 0;
		double reserved_until = 0;
	};
	struct Roof {
		Node3D *node = nullptr;
		Vector3 center, size;
	};
	struct MissionClaim {
		int owner = 0;
		double until = 0;
		float bid = 0;
		Vector3 position;
		String label;
		Vector3 peer_position;
	};
	std::map<int, MissionClaim> mission_claims;
	int decision_cursor = 0;
	bool compound_paths_ready = false;
	uint64_t compound_nav_iteration = 0;
	std::vector<CoverPoint> cover_points;
	std::vector<Roof> roofs;
	bool compound_world = false, overview = false;
	float master_volume = .6f, overview_zoom = 80, previous_view_yaw = 0, previous_view_pitch = -.3f;
	double beacon_times[2] = { -1, -1 };
	bool beacon_active[2] = { false, false };
	bool beacon_contested[2] = { false, false };
	float beacon_hold[2] = { 0, 0 };
	int wave_stage = 0;
	double next_wave = 0;
	void create_compound_level();
	void create_classic_level();
	void perceive_compound(Actor &actor);
	void step_compound_ai();
	void refresh_compound_routes();
	void decide_compound(Actor &actor);
	void follow_compound_plan(Actor &actor);
	void build_compound_plan(Actor &actor);
	void update_compound(float dt);
	void evaluate_positions(Actor &actor, const Vector3 &threat);
	void shoot_compound(Actor &actor);
	void add_cover_box(const Vector3 &center, const Vector3 &size, const String &label, float rotation = 0);
	void update_equipment_pose(Actor &actor);
	void add_compound_gear(Actor &actor);
	Node3D *make_compound_pickup(const String &kind, const Vector3 &position);
	void spawn_gameplay_effect(const String &kind, const Vector3 &position, int owner, double until);
	void snapshot_compound(Dictionary &data) const;
	bool validate_compound_save(const Dictionary &data, std::vector<Actor> &candidate, double time) const;
	void restore_compound(const Dictionary &data);
	bool issue_order(int id, int order);
	void compound_integration_step();
	void play_spatial(const String &kind, const Vector3 &where, float pitch = 1);
	bool arctic = false, power_on = false;
	Vector3 generator_position, alternative_exit, gate_closed_position = Vector3(20, 1.5, 14);
	float world_extent = 40;
	String session_id;
	double next_profile_save = 0;
	int inspector_actor = 2;
	void create_arctic_level();
	void create_presentation();
	void decide_strategy(Actor &actor);
	void decide_distractor(Actor &actor);
	void build_plan(Actor &actor);
	int nearest_region(const Vector3 &position) const;
	Vector3 mission_target_for(const Actor &actor) const;
	void create_arctic_art();
	void add_item(const String &kind, const Vector3 &position, int container = 0);
	int add_world_object(const String &kind, const String &name, const Vector3 &position);
	bool interact_environment(Actor &actor);
	void complete_environment_action(Actor &actor);
	void update_environment(float dt);
	void set_door(WorldObject &door, bool open);
	bool acquire_item(Actor &actor, Item &item);
	void use_equipment(Actor &actor, const String &kind);
	void load_profiles();
	bool save_profiles();
	bool reset_profile(int id);
	void record_experience(Actor &actor, const String &kind, int peer = 0, float reward = 0);
	Dictionary profile_info(const Actor &actor) const;
	void learn_tick(Actor &actor);
	std::vector<Actor> actors;
	std::vector<Item> items;
	std::vector<String> map_rows;
	NavigationRegion3D *region = nullptr;
	StaticBody3D *gate = nullptr;
	MeshInstance3D *gate_mesh = nullptr;
	Node3D *gate_model = nullptr;
	float gate_lift = 0;
	int gate_operator_a = 0, gate_operator_b = 0;
	bool gate_opening = false;
	Node3D *camera_rig = nullptr;
	SpringArm3D *arm = nullptr;
	Camera3D *camera = nullptr;
	Label *hud = nullptr;
	Label *message_label = nullptr;
	Label *legacy_crosshair = nullptr;
	bool frontend_attached = false, session_started = true, automated_run = false;
	String frontend_mode = "playing", interaction_hint;
	float mouse_sensitivity = .0025f;
	bool invert_mouse = false;
	int event_sequence = 0, cooperative_uses = 0, harmful_attacks = 0;
	Dictionary scene_cache;
	Ref<TCPServer> python_server;
	Ref<StreamPeerTCP> python_peer;
	int64_t python_pid = 0;
	String python_buffer, python_token;
	int request_id = 0, pending_request = 0;
	double request_time = 0;
	bool python_ready = false;
	std::map<int, String> policy;
	std::map<int, std::set<String>> request_masks;
	Dictionary config, assets;
	std::mt19937 rng;
	int seed = 42, mission = 0, spawn_count = 2;
	bool reported = false;
	bool integration_ok = true;
	Ref<FileAccess> trace_file;
	Ref<FileAccess> neural_trace;
	lastlight::NeuralPolicy neural_policy;
	std::mt19937 neural_rng;
	String neural_checkpoint, neural_path, neural_trace_path;
	bool fixed_profiles = false, controller_explicit = false, neural_trace_failed = false;
	float neural_temperature = 0.f;
	std::array<int, 3> neural_decisions{}, neural_fallbacks{};
	bool load_native_policy(const String &path);
	bool neural_choices_enabled(const Actor &a) const;
	lastlight::NeuralFeatures neural_context(const Actor &a) const;
	void neural_target(lastlight::NeuralFeatures &features, const Actor &a, const Vector3 &target) const;
	void neural_item(lastlight::NeuralFeatures &features, const String &kind) const;
	int neural_choose(const Actor &a, std::vector<lastlight::NeuralChoice> &choices, int teacher, int stage);
	void neural_outcome();
	void neural_status(Dictionary &data) const;
	void integration_step();
	bool gate_open = false, key_found = false, radio_done = false, paused = false, victory = false, defeat = false, autoplay = false, smoke = false, debug_overlay = false;
	double elapsed = 0, ai_clock = 0, hud_clock = 0, gate_hold = 0;
	int camera_focus = 1;
	int physics_frames = 0, max_frames = 18000;
	int shots_hit = 0, wall_blocks = 0, revivals = 0, pickups = 0, python_failures = 0;
	float camera_yaw = -2.35619449f, camera_pitch = -.3f;
	String policy_path, controller = "rule";
	String message, report_path, script_case, python_executable, save_path = "user://save.json";
	double message_time = 0;
	Array journal;
	Vector3 key_position, radio_position, exit_position, control_a, control_b;
	Node3D *level_root = nullptr;
	static void _bind_methods();
	void _notification(int what);
	void input(const Ref<InputEvent> &event) override;
	void setup();
	void create_level();
	void create_actor(int id, const Vector3 &position, bool survivor, const Color &color, int role = 0);
	void physics(float dt);
	void decide(Actor &a);
	void move_actor(Actor &a, float dt);
	void animate(Actor &a, float dt, const Vector3 &delta);
	bool visible(const Actor &a, const Actor &b, float range = 18) const;
	bool clear_line(const Vector3 &from, const Vector3 &to) const;
	bool can_see(const Vector3 &from, const Vector3 &to) const;
	Actor *find_actor(int id);
	const Actor *find_actor(int id) const;
	void shoot(Actor &a);
	void melee(Actor &a);
	void interact(Actor &a);
	void emit_event(int actor, int target, const String &kind, const Vector3 &where);
	void update_objectives(float dt);
	void update_hud();
	void notify(const String &text);
	void restart();
	Node3D *make_box(Node3D *parent, const Vector3 &position, const Vector3 &size, const Color &color, bool collision = false);
	void try_asset(Actor &a);
	void decorate_level();
	Node3D *place_asset(const String &path, const Vector3 &position, const Vector3 &size, bool collision = false);
	Ref<Material> level_material(bool floor);
	Ref<PackedScene> load_asset_scene(const String &path);
	void setup_animation_graph(Actor &a);
	void update_animation_graph(Actor &a, float dt, const Vector3 &velocity);
	void configure_bots(int count);
	void sense(Actor &a);
	Dictionary build_observation(const Actor &a) const;
	void apply_melee(Actor &a);
	void start_python();
	void poll_python();
	void request_policy();
	void send_python(const Dictionary &data);
	void stop_python();
	Dictionary snapshot() const;
	bool restore(const Dictionary &data);
	bool save_game();
	bool load_game();
	void finish_report();

public:
	void attach_frontend();
	Dictionary get_game_state() const;
	bool game_command(const String &command, const Dictionary &payload = Dictionary());
	void apply_options(const Dictionary &options);
	LastLightGame() = default;
	~LastLightGame();
};
