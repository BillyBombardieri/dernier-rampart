extends Node
## Vérification rapide : charge chaque script et chaque scène (les autoloads sont actifs).
## Lancer : godot --headless --path . res://tests/load_check.tscn

func _ready() -> void:
	var failed := 0
	for dir_path in ["res://scripts", "res://tests"]:
		var dir := DirAccess.open(dir_path)
		for f in dir.get_files():
			if f.ends_with(".gd"):
				var s: Script = load(dir_path + "/" + f)
				if s == null or not s.can_instantiate():
					print("ERREUR de chargement : ", f)
					failed += 1
	for scene in ["res://scenes/menu.tscn", "res://scenes/main.tscn"]:
		var packed: PackedScene = load(scene)
		if packed == null:
			print("ERREUR de scène : ", scene)
			failed += 1
	print("Vérification terminée : ", failed, " erreur(s)")
	get_tree().quit(1 if failed > 0 else 0)
