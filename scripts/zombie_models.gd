## Généré par tools/make_models.py : ne pas modifier à la main.
## Repères des modèles de zombies, en mètres dans l'espace du modèle importé (Y vers le haut,
## le zombie regarde vers +Z). walk et run : vitesse au sol d'un cycle d'animation joué à
## vitesse normale, pour un modèle à l'échelle 1 (0 = pas d'animation de course).

## Moment où le coup porte dans l'animation "attack" et où part le crachat dans "spit"
## (fraction de la durée de l'animation).
const ATTACK_HIT := 0.42
const SPIT_RELEASE := 0.5

const DATA := {
	"rodeur": {"height": 1.800, "eyes": [Vector3(0.0320, 1.6910, 0.0845), Vector3(-0.0320, 1.6910, 0.0845)], "eye_radius": 0.0135, "walk": 1.585, "run": 0.000},
	"rodeur_b": {"height": 1.785, "eyes": [Vector3(0.0326, 1.6741, 0.0861), Vector3(-0.0326, 1.6741, 0.0861)], "eye_radius": 0.0138, "walk": 1.453, "run": 0.000},
	"rodeur_c": {"height": 1.815, "eyes": [Vector3(0.0314, 1.7079, 0.0829), Vector3(-0.0314, 1.7079, 0.0829)], "eye_radius": 0.0132, "walk": 1.429, "run": 0.000},
	"coureur": {"height": 1.839, "eyes": [Vector3(0.0310, 1.7332, 0.0821), Vector3(-0.0310, 1.7332, 0.0821)], "eye_radius": 0.0131, "walk": 1.474, "run": 3.750},
	"brute": {"height": 1.772, "eyes": [Vector3(0.0336, 1.6573, 0.0885), Vector3(-0.0336, 1.6573, 0.0885)], "eye_radius": 0.0142, "walk": 0.923, "run": 0.000},
	"cracheur": {"height": 1.800, "eyes": [Vector3(0.0320, 1.6910, 0.0845), Vector3(-0.0320, 1.6910, 0.0845)], "eye_radius": 0.0135, "walk": 1.524, "run": 0.000},
	"hurleur": {"height": 1.836, "eyes": [Vector3(0.0326, 1.7248, 0.0861), Vector3(-0.0326, 1.7248, 0.0861)], "eye_radius": 0.0138, "walk": 1.359, "run": 0.000},
	"fouisseur": {"height": 1.741, "eyes": [Vector3(0.0320, 1.6319, 0.0845), Vector3(-0.0320, 1.6319, 0.0845)], "eye_radius": 0.0135, "walk": 1.970, "run": 0.000},
	"boss": {"height": 1.783, "eyes": [Vector3(0.0346, 1.6657, 0.0909), Vector3(-0.0346, 1.6657, 0.0909)], "eye_radius": 0.0146, "walk": 0.469, "run": 0.000},
}
