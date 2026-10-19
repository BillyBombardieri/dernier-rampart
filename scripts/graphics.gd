class_name Graphics
extends RefCounted
## Qualité du rendu, réglable (Échap > Réglages) : Moyenne pour les PC modestes, Haute, et Ultra
## qui vise le rendu des jeux sous Unreal Engine 5. Haute ajoute l'anticrénelage temporel, les
## reflets à l'écran (asphalte mouillé, flaques, métal), des ombres plus douces, un brouillard
## volumétrique plus fin et un post-traitement « caméra » ; Ultra ajoute l'éclairage global
## (la lumière des feux, des lampes et du ciel rebondit sur les murs et le sol, comme Lumen).

const NAMES := ["Moyenne", "Haute", "Ultra"]
const MEDIUM := 0
const HIGH := 1
const ULTRA := 2


## Réglages globaux du moteur et de la fenêtre : anticrénelage, ombres, brouillard.
static func apply_viewport(vp: Viewport) -> void:
	var q: int = Settings.quality
	vp.use_taa = q >= HIGH
	vp.screen_space_aa = Viewport.SCREEN_SPACE_AA_FXAA if q == MEDIUM else Viewport.SCREEN_SPACE_AA_DISABLED
	vp.anisotropic_filtering_level = Viewport.ANISOTROPY_16X if q >= HIGH else Viewport.ANISOTROPY_4X
	var soft: RenderingServer.ShadowQuality = [RenderingServer.SHADOW_QUALITY_SOFT_LOW, RenderingServer.SHADOW_QUALITY_SOFT_MEDIUM, RenderingServer.SHADOW_QUALITY_SOFT_HIGH][q]
	RenderingServer.directional_soft_shadow_filter_set_quality(soft)
	RenderingServer.positional_soft_shadow_filter_set_quality(soft)
	RenderingServer.directional_shadow_atlas_set_size(8192 if q == ULTRA else 4096, true)
	RenderingServer.environment_set_volumetric_fog_volume_size([64, 128, 160][q], [64, 96, 128][q])
	RenderingServer.environment_set_volumetric_fog_filter_active(q >= HIGH)
	RenderingServer.environment_set_ssao_quality(
		[RenderingServer.ENV_SSAO_QUALITY_MEDIUM, RenderingServer.ENV_SSAO_QUALITY_HIGH, RenderingServer.ENV_SSAO_QUALITY_HIGH][q],
		q < ULTRA, 0.5, 2, 50.0, 300.0)
	RenderingServer.environment_set_ssr_roughness_quality(RenderingServer.ENV_SSR_ROUGHNESS_QUALITY_MEDIUM if q == ULTRA else RenderingServer.ENV_SSR_ROUGHNESS_QUALITY_LOW)
	RenderingServer.environment_set_sdfgi_ray_count(RenderingServer.ENV_SDFGI_RAY_COUNT_32)
	RenderingServer.environment_set_sdfgi_frames_to_converge(RenderingServer.ENV_SDFGI_CONVERGE_IN_15_FRAMES)
	RenderingServer.environment_set_sdfgi_frames_to_update_light(RenderingServer.ENV_SDFGI_UPDATE_LIGHT_IN_4_FRAMES)


## Effets de l'ambiance d'une carte : reflets à l'écran et éclairage global.
static func apply_environment(env: Environment) -> void:
	var q: int = Settings.quality
	env.ssr_enabled = q >= HIGH
	env.ssr_max_steps = 96 if q == ULTRA else 64
	env.ssr_fade_in = 0.15
	env.ssr_fade_out = 2.0
	env.ssr_depth_tolerance = 0.3
	env.sdfgi_enabled = q == ULTRA
	env.sdfgi_use_occlusion = true
	env.sdfgi_cascades = 4
	env.sdfgi_min_cell_size = 0.25
	env.sdfgi_y_scale = Environment.SDFGI_Y_SCALE_75_PERCENT
	env.sdfgi_bounce_feedback = 0.5
	env.sdfgi_energy = 0.9


## Post-traitement « caméra » au-dessus de la scène et sous l'interface : vignettage, grain
## fin, légère aberration sur les bords et netteté rendue après l'anticrénelage temporel.
static func post_layer() -> CanvasLayer:
	var layer := CanvasLayer.new()
	layer.layer = -10
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var mat := ShaderMaterial.new()
	mat.shader = preload("res://scripts/camera_post.gdshader")
	rect.material = mat
	layer.add_child(rect)
	layer.visible = Settings.quality >= HIGH
	layer.add_to_group("post_camera")
	return layer
