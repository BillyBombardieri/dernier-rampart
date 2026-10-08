class_name Sfx
extends RefCounted
## Sons du jeu (assets/sounds), joués dans l'espace 3D ou à plat pour l'interface.

static var _streams := {}


static func stream(name: String) -> AudioStream:
	if not _streams.has(name):
		var path := "res://assets/sounds/%s.wav" % name
		_streams[name] = load(path) if ResourceLoader.exists(path) else null
	return _streams[name]


## Son positionné dans le monde (il s'atténue avec la distance).
static func play_at(parent: Node, name: String, pos: Vector3, volume_db := 0.0, pitch_jitter := 0.08, max_distance := 80.0) -> void:
	var s := stream(name)
	if s == null:
		return
	var p := AudioStreamPlayer3D.new()
	p.stream = s
	p.volume_db = volume_db
	p.max_distance = max_distance
	p.unit_size = 6.0
	p.pitch_scale = 1.0 + randf_range(-pitch_jitter, pitch_jitter)
	parent.add_child(p)
	p.global_position = pos
	p.finished.connect(p.queue_free)
	p.play()


## Son non spatialisé (armes du joueur, interface).
static func play(parent: Node, name: String, volume_db := 0.0, pitch_jitter := 0.05) -> void:
	var s := stream(name)
	if s == null:
		return
	var p := AudioStreamPlayer.new()
	p.stream = s
	p.volume_db = volume_db
	p.pitch_scale = 1.0 + randf_range(-pitch_jitter, pitch_jitter)
	parent.add_child(p)
	p.finished.connect(p.queue_free)
	p.play()
