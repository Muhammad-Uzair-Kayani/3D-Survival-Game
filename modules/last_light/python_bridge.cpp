#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "last_light_game.h"
#include <set>
void LastLightGame::start_python() {
	python_server.instantiate();
	if (python_server->listen(0, IPAddress("127.0.0.1")) != OK) {
		++python_failures;
		python_server.unref();
		return;
	}
	python_token = itos(OS::get_singleton()->get_ticks_usec()) + "-" + itos(seed);
	String script = OS::get_singleton()->get_executable_path().get_base_dir().path_join("python/service.py");
	if (!FileAccess::exists(script)) {
		script = ProjectSettings::get_singleton()->globalize_path("res://../python/service.py");
	}
	List<String> args;
	args.push_back("-u");
	args.push_back(script);
	args.push_back("--port");
	args.push_back(itos(python_server->get_local_port()));
	args.push_back("--token");
	args.push_back(python_token);
	if (smoke && script_case.begins_with("python_fault_")) {
		args.push_back("--test-fault");
		args.push_back(script_case.trim_prefix("python_fault_"));
	}
	OS::ProcessID pid = 0;
	if (OS::get_singleton()->create_process(python_executable, args, &pid, false) != OK) {
		++python_failures;
		python_server.unref();
		notify("Python unavailable; authored AI enabled");
		return;
	}
	python_pid = pid;
	request_time = OS::get_singleton()->get_ticks_msec() / 1000.;
}
void LastLightGame::stop_python() {
	// Detach first: a failed shutdown write must not recursively call stop_python.
	Ref<StreamPeerTCP> peer = python_peer;
	python_peer.unref();
	python_buffer = String();
	if (peer.is_valid() && peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
		Dictionary message;
		message["v"] = 1;
		message["type"] = "shutdown";
		message["request_id"] = 0;
		message["episode"] = 0;
		CharString bytes = (JSON::stringify(message) + "\n").utf8();
		peer->put_data(reinterpret_cast<const uint8_t *>(bytes.get_data()), bytes.length());
	}
	if (peer.is_valid()) {
		peer->disconnect_from_host();
	}
	if (python_server.is_valid()) {
		python_server->stop();
		python_server.unref();
	}
	if (python_pid && OS::get_singleton()->is_process_running(python_pid)) {
		OS::get_singleton()->kill(python_pid);
	}
	python_pid = 0;
	python_ready = false;
	pending_request = 0;
	policy.clear();
	request_masks.clear();
}
void LastLightGame::send_python(const Dictionary &data) {
	if (python_peer.is_null()) {
		return;
	}
	CharString bytes = (JSON::stringify(data) + "\n").utf8();
	if (python_peer->put_data(reinterpret_cast<const uint8_t *>(bytes.get_data()), bytes.length()) != OK) {
		++python_failures;
		stop_python();
	}
}
void LastLightGame::poll_python() {
	if (python_server.is_null()) {
		return;
	}
	double now = OS::get_singleton()->get_ticks_msec() / 1000.;
	if (python_pid && !OS::get_singleton()->is_process_running(python_pid)) {
		// The process check may reap a dead child. Do not probe that PID again.
		python_pid = 0;
		++python_failures;
		stop_python();
		notify("Python process exited; authored AI continues");
		return;
	}
	if (python_peer.is_null() && python_server->is_connection_available()) {
		python_peer = python_server->take_connection();
		python_peer->set_no_delay(true);
	}
	if (python_peer.is_valid()) {
		python_peer->poll();
		if (python_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
			++python_failures;
			stop_python();
			return;
		}
		int available = python_peer->get_available_bytes();
		if (available > 0) {
			if (available > 65536) {
				++python_failures;
				stop_python();
				return;
			}
			Vector<uint8_t> bytes;
			bytes.resize(available);
			int got = 0;
			if (python_peer->get_partial_data(bytes.ptrw(), available, got) != OK) {
				++python_failures;
				stop_python();
				return;
			}
			python_buffer += String::utf8(reinterpret_cast<const char *>(bytes.ptr()), got);
		}
		while (python_buffer.contains("\n")) {
			int end = python_buffer.find("\n");
			String line = python_buffer.substr(0, end);
			python_buffer = python_buffer.substr(end + 1);
			Variant parsed = JSON::parse_string(line);
			if (parsed.get_type() != Variant::DICTIONARY) {
				++python_failures;
				stop_python();
				return;
			}
			Dictionary reply = parsed;
			String type = reply.get("type", String());
			if (int(reply.get("v", 0)) != 1) {
				++python_failures;
				stop_python();
				return;
			}
			if (type == "connected" && !python_ready) {
				if (String(reply.get("token", String())) != python_token) {
					++python_failures;
					stop_python();
					return;
				}
				Dictionary load;
				load["v"] = 1;
				load["type"] = "load_policy";
				load["request_id"] = 0;
				load["episode"] = 0;
				auto file = FileAccess::open(policy_path.is_empty() ? String("res://content/champion.json") : policy_path, FileAccess::READ);
				if (file.is_valid()) {
					Variant p = JSON::parse_string(file->get_as_text());
					if (p.get_type() == Variant::DICTIONARY) {
						load["weights"] = Dictionary(p).get("weights", Array());
					}
				}
				send_python(load);
				continue;
			}
			if (type == "loaded") {
				python_ready = true;
				pending_request = 0;
				continue;
			}
			if (type != "intents" || int(reply.get("request_id", -1)) != pending_request || int(reply.get("episode", -1)) != seed) {
				++python_failures;
				stop_python();
				return;
			}
			Array actions = reply.get("actions", Array());
			std::map<int, String> accepted;
			int expected = request_masks.size();
			bool valid = actions.size() == expected;
			for (int n = 0; n < actions.size(); ++n) {
				if (actions[n].get_type() != Variant::DICTIONARY) {
					valid = false;
					break;
				}
				Dictionary action = actions[n];
				Variant raw_id = action.get("id", Variant());
				if ((raw_id.get_type() != Variant::INT && raw_id.get_type() != Variant::FLOAT) || !std::isfinite(double(raw_id)) || double(raw_id) < 1 || double(raw_id) > 100 || Math::floor(double(raw_id)) != double(raw_id)) {
					valid = false;
					break;
				}
				int id = int(raw_id);
				String intent = action.get("intent", String());
				auto actor = find_actor(id);
				if (!actor || !actor->stats.survivor || !request_masks.count(id) || !request_masks[id].count(intent) || accepted.count(id) || (intent != "Explore" && intent != "Acquire" && intent != "Cooperate" && intent != "Avoid" && intent != "Defend" && intent != "Progress") || int(action.get("target_id", 0)) != 0) {
					valid = false;
					break;
				}
				accepted[id] = intent;
			}
			if (!valid) {
				++python_failures;
				stop_python();
				return;
			}
			policy = accepted;
			pending_request = 0;
		}
	}
	if (python_buffer.length() > 65536 || (!python_ready && now - request_time > 3) || (pending_request && now - request_time > 1)) {
		++python_failures;
		notify("Python timeout/invalid reply; authored AI continues");
		stop_python();
	}
}
void LastLightGame::request_policy() {
	if (!python_ready || pending_request) {
		return;
	}
	Array observations;
	request_masks.clear();
	for (auto &a : actors) {
		if (a.stats.survivor && a.stats.active()) {
			Dictionary o = build_observation(a);
			Array mask = o["valid_actions"];
			const char *names[] = { "Explore", "Acquire", "Cooperate", "Avoid", "Defend", "Progress" };
			for (int k = 0; k < mask.size(); ++k) {
				if (int(mask[k])) {
					request_masks[a.stats.id].insert(names[k]);
				}
			}
			observations.push_back(o);
		}
	}
	if (observations.is_empty()) {
		return;
	}
	Dictionary q;
	q["v"] = 1;
	q["type"] = "infer_batch";
	q["request_id"] = ++request_id;
	q["episode"] = seed;
	q["agents"] = observations;
	pending_request = request_id;
	request_time = OS::get_singleton()->get_ticks_msec() / 1000.;
	send_python(q);
}
