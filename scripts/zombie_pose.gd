class_name ZombiePose
extends SkeletonModifier3D
## Réactions ajoutées par-dessus l'animation d'un zombie : il encaisse les balles (buste et tête
## rejetés en arrière, tournés vers le côté touché) et vacille quand il est étourdi.
## Axes des os (modèles de tools/make_models.py) : Y le long de l'os, Z vers l'avant du corps,
## X vers sa gauche. Une rotation positive autour de X penche l'os vers l'avant.

var flinch := 0.0  ## 0 à 1 : force du recul après un impact.
var twist := 0.0  ## -1 à 1 : côté de l'impact, le buste pivote avec lui.
var dizzy := 0.0  ## 0 à 1 : étourdi, le buste et la tête oscillent.

var _time := randf() * 10.0
var _bones := PackedInt32Array()


func _process_modification_with_delta(delta: float) -> void:
	var sk := get_skeleton()
	if sk == null:
		return
	_time += delta
	if _bones.is_empty():
		for bone in ["spine", "chest", "neck", "head"]:
			_bones.append(sk.find_bone(bone))
	if flinch < 0.001 and dizzy < 0.001:
		return
	var sway := sin(_time * 11.0) * dizzy
	var nod := sin(_time * 7.0) * dizzy
	_turn(sk, _bones[0], -0.2 * flinch, 0.25 * twist * flinch, 0.12 * sway)
	_turn(sk, _bones[1], -0.15 * flinch, 0.15 * twist * flinch, 0.08 * sway)
	_turn(sk, _bones[2], -0.1 * flinch, 0.0, 0.12 * nod)
	_turn(sk, _bones[3], -0.35 * flinch, 0.2 * twist * flinch, 0.3 * nod)


## Tourne un os dans son propre repère : pitch autour de X, yaw autour de l'os, roll autour de Z.
func _turn(sk: Skeleton3D, bone: int, pitch: float, yaw: float, roll: float) -> void:
	if bone < 0:
		return
	var q := Quaternion.from_euler(Vector3(pitch, yaw, roll))
	sk.set_bone_pose_rotation(bone, sk.get_bone_pose_rotation(bone) * q)
