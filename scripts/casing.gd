class_name Casing
extends MeshInstance3D
## Douille éjectée par l'arme : elle tourne en l'air, rebondit une ou deux fois sur le sol,
## reste couchée quelques secondes puis disparaît. Au plus MAX douilles à la fois.

const MAX := 40
const LIFE := 12.0
const GRAVITY := 9.8

static var _mat: StandardMaterial3D
static var _mesh: CylinderMesh

var velocity := Vector3.ZERO
var _spin := Vector3.ZERO
var _bounces := 0
var _rest := false
var _age := 0.0


static func eject(parent: Node, pos: Vector3, vel: Vector3, rifle: bool) -> void:
	var old := parent.get_tree().get_nodes_in_group("casings")
	if old.size() >= MAX:
		old[0].queue_free()
	var c := Casing.new()
	c.add_to_group("casings")
	c.mesh = _get_mesh()
	c.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	c.scale = Vector3(1.0, 1.6, 1.0) if rifle else Vector3.ONE
	c.velocity = vel
	c._spin = Vector3(randf_range(-25, 25), randf_range(-8, 8), randf_range(-25, 25))
	parent.add_child(c)
	c.global_position = pos
	c.rotation = Vector3(randf() * TAU, randf() * TAU, randf() * TAU)


static func _get_mesh() -> CylinderMesh:
	if _mesh == null:
		_mat = StandardMaterial3D.new()
		_mat.albedo_color = Color(0.78, 0.6, 0.3)
		_mat.metallic = 0.9
		_mat.roughness = 0.32
		_mesh = CylinderMesh.new()
		_mesh.top_radius = 0.0055
		_mesh.bottom_radius = 0.0058
		_mesh.height = 0.019
		_mesh.radial_segments = 8
		_mesh.rings = 1
		_mesh.material = _mat
	return _mesh


func _physics_process(delta: float) -> void:
	_age += delta
	if _age > LIFE:
		queue_free()
		return
	if _rest:
		return
	velocity.y -= GRAVITY * delta
	global_position += velocity * delta
	rotation += _spin * delta
	if global_position.y <= 0.02 and velocity.y < 0.0:
		global_position.y = 0.02
		_bounces += 1
		if _bounces >= 3 or absf(velocity.y) < 0.6:
			# Couchée sur le flanc.
			_rest = true
			rotation = Vector3(PI * 0.5, randf() * TAU, 0.0)
			global_position.y = 0.006 * scale.y
			return
		velocity = Vector3(velocity.x * 0.45, -velocity.y * 0.32, velocity.z * 0.45)
		_spin *= 0.5
