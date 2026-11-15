extends LastLightGame

const GOLD := Color("edba70")
const TEAL := Color("67c6b7")
const INK := Color("111b24")
const TEXT := Color("ecf0ef")
const MUTED := Color("9aabb7")
const MiniMap = preload("res://ui/minimap.gd")

var canvas: CanvasLayer
var root: Control
var modal: PanelContainer
var modal_body: VBoxContainer
var shade: ColorRect
var hud_root: Control
var objective_label: Label
var objective_detail: Label
var stat_label: Label
var health_bar: ProgressBar
var stamina_bar: ProgressBar
var hunger_bar: ProgressBar
var ammo_label: Label
var prompt_label: Label
var toast_label: Label
var action_label: Label
var squad_rows: Array[Label] = []
var debug_label: Label
var map_control: Control
var tactical_map_control: Control
var state: Dictionary = {}
var visible_mode := ""
var panel_screen := ""
var refresh := 0.0
var selected_bots := 5
var inspected_id := 2
var inventory_labels: Dictionary = {}
var profile_details: RichTextLabel
var reset_new_game := false
var profile_refresh := 0.0
var sensitivity := 0.0025
var invert_y := false
var resolution := 0
var volume := 0.6
var fps_cap := 30
var last_sound_event := -1
var audio: AudioStreamPlayer
var sounds: Dictionary = {}
var last_player_health := 100
var flash: ColorRect
var modal_status: Label
var crosshair: Label
var tutorial_tip: Label

func _ready() -> void:
    call_deferred("_build_frontend")

func _style(color: Color, radius: int = 10, border: Color = Color.TRANSPARENT) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color=color
    style.corner_radius_top_left=radius
    style.corner_radius_top_right=radius
    style.corner_radius_bottom_left=radius
    style.corner_radius_bottom_right=radius
    style.border_color=border
    style.set_border_width_all(1 if border.a>0 else 0)
    style.content_margin_left=18
    style.content_margin_right=18
    style.content_margin_top=14
    style.content_margin_bottom=14
    return style

func _label(text: String, font_size: int = 16, color: Color = TEXT) -> Label:
    var label := Label.new()
    label.text=text
    label.add_theme_font_size_override("font_size",font_size)
    label.add_theme_color_override("font_color",color)
    return label

func _button(text: String, callback: Callable, primary: bool = false) -> Button:
    var button := Button.new()
    button.text=text
    button.custom_minimum_size=Vector2(0,40)
    button.add_theme_font_size_override("font_size",16)
    for state_name in ["font_color","font_hover_color","font_pressed_color","font_focus_color"]:
        button.add_theme_color_override(state_name,INK if primary else TEXT)
    button.add_theme_stylebox_override("normal",_style(GOLD if primary else Color("24333f"),6))
    button.add_theme_stylebox_override("hover",_style(Color("f5ce94") if primary else Color("334b58"),6))
    button.add_theme_stylebox_override("pressed",_style(TEAL if primary else Color("3b5a66"),6))
    button.pressed.connect(callback)
    return button

func _panel(parent: Node, anchor: int, position: Vector2, minimum: Vector2) -> PanelContainer:
    var panel := PanelContainer.new()
    parent.add_child(panel)
    panel.set_anchors_and_offsets_preset(anchor)
    panel.offset_left=position.x
    panel.offset_top=position.y
    panel.offset_right=position.x+minimum.x
    panel.offset_bottom=position.y+minimum.y
    panel.custom_minimum_size=minimum
    panel.add_theme_stylebox_override("panel",_style(Color(INK,0.9),9,Color("344753")))
    panel.mouse_filter=Control.MOUSE_FILTER_IGNORE
    return panel

func _build_frontend() -> void:
    if Engine.is_editor_hint(): return
    attach_frontend()
    if DisplayServer.get_name()=="headless":return
    _load_options()
    canvas=CanvasLayer.new()
    canvas.layer=20
    add_child(canvas)
    root=Control.new()
    canvas.add_child(root)
    root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    root.mouse_filter=Control.MOUSE_FILTER_IGNORE
    var theme := Theme.new()
    theme.default_font_size=16
    root.theme=theme
    hud_root=Control.new()
    root.add_child(hud_root)
    hud_root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    hud_root.mouse_filter=Control.MOUSE_FILTER_IGNORE
    _build_hud()
    shade=ColorRect.new()
    root.add_child(shade)
    shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    shade.color=Color(0.025,0.045,0.065,0.79)
    modal=PanelContainer.new()
    root.add_child(modal)
    modal.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    modal.custom_minimum_size=Vector2(440,0)
    modal.add_theme_stylebox_override("panel",_style(Color("15232e"),14,Color("3c4d58")))
    modal_body=VBoxContainer.new()
    modal_body.add_theme_constant_override("separation",10)
    modal.resized.connect(_layout)
    modal.add_child(modal_body)
    audio=AudioStreamPlayer.new()
    add_child(audio)
    audio.volume_db=linear_to_db(maxf(volume,0.001))
    for name in ["pickup","radio","hurt","shot","gate","revive"]:
        var path: String = "res://audio/"+str(name)+".wav"
        if FileAccess.file_exists(path): sounds[name]=AudioStreamWAV.load_from_buffer(FileAccess.get_file_as_bytes(path))
    get_viewport().size_changed.connect(_layout)
    get_window().focus_exited.connect(func():
        if state.get("mode","")=="playing":game_command("pause"))
    root.move_child(toast_label,root.get_child_count()-1)
    _layout()
    _refresh_state()

func _build_hud() -> void:
    var objective_panel := _panel(hud_root,Control.PRESET_TOP_LEFT,Vector2(16,16),Vector2(350,74))
    var objective_box := VBoxContainer.new()
    objective_panel.add_child(objective_box)
    objective_box.add_child(_label("LAST LIGHT   /   SURVIVAL",12,GOLD))
    objective_label=_label("Recover the yard key",20)
    objective_box.add_child(objective_label)
    objective_detail=_label("",13,MUTED)
    objective_box.add_child(objective_detail)
    var stats := _panel(hud_root,Control.PRESET_BOTTOM_LEFT,Vector2(16,-126),Vector2(320,110))
    var stats_box := VBoxContainer.new()
    stats.add_child(stats_box)
    stat_label=_label("YOU",13,GOLD)
    stats_box.add_child(stat_label)
    health_bar=_bar(stats_box,Color("d98983"))
    stamina_bar=_bar(stats_box,TEAL)
    hunger_bar=_bar(stats_box,GOLD)
    var ammo := _panel(hud_root,Control.PRESET_BOTTOM_RIGHT,Vector2(-194,-105),Vector2(178,89))
    var ammo_box := VBoxContainer.new()
    ammo.add_child(ammo_box)
    ammo_label=_label("UNARMED",20,GOLD)
    ammo_box.add_child(ammo_label)
    action_label=_label("",13,MUTED)
    ammo_box.add_child(action_label)
    var squad := _panel(hud_root,Control.PRESET_TOP_RIGHT,Vector2(-240,16),Vector2(224,170))
    var squad_box := VBoxContainer.new()
    squad.add_child(squad_box)
    squad_box.add_child(_label("SURVIVORS",12,GOLD))
    for i in range(4):
        var row := _label("",13)
        row.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
        squad_box.add_child(row)
        squad_rows.append(row)
    map_control=MiniMap.new()
    hud_root.add_child(map_control)
    map_control.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
    map_control.offset_left=-210
    map_control.offset_right=-20
    map_control.offset_top=206
    map_control.offset_bottom=360
    map_control.custom_minimum_size=Vector2(190,154)
    map_control.size=Vector2(190,154)
    map_control.mouse_filter=Control.MOUSE_FILTER_IGNORE
    prompt_label=_label("",17,GOLD)
    hud_root.add_child(prompt_label)
    prompt_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM)
    prompt_label.offset_left=-220
    prompt_label.offset_right=220
    prompt_label.offset_top=-162
    prompt_label.offset_bottom=-134
    prompt_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
    toast_label=_label("",14,TEXT)
    toast_label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
    root.add_child(toast_label)
    toast_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
    toast_label.offset_left=-220
    toast_label.offset_right=220
    toast_label.offset_top=115
    toast_label.offset_bottom=155
    toast_label.custom_minimum_size=Vector2(400,0)
    toast_label.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
    crosshair = _label("·",32,Color(1,1,1,.8))
    hud_root.add_child(crosshair)
    crosshair.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    crosshair.offset_left=-6
    crosshair.offset_right=6
    crosshair.offset_top=-22
    crosshair.offset_bottom=22
    debug_label=_label("",13,TEAL)
    hud_root.add_child(debug_label)
    debug_label.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT)
    debug_label.offset_left=16
    debug_label.offset_right=600
    debug_label.offset_top=-212
    debug_label.offset_bottom=-165
    tutorial_tip=_label("WASD · E Interact · F2 View · Z Cover · J Inventory · F3 Squad · I Profiles · M Map",12,MUTED)
    hud_root.add_child(tutorial_tip)
    tutorial_tip.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM)
    tutorial_tip.offset_left=-360
    tutorial_tip.offset_right=360
    tutorial_tip.offset_top=-18
    tutorial_tip.offset_bottom=0
    tutorial_tip.horizontal_alignment=HORIZONTAL_ALIGNMENT_CENTER
    flash=ColorRect.new()
    hud_root.add_child(flash)
    flash.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    flash.color=Color(0.7,0.08,0.06,0)
    flash.mouse_filter=Control.MOUSE_FILTER_IGNORE

func _bar(parent: Node, color: Color) -> ProgressBar:
    var bar := ProgressBar.new()
    bar.custom_minimum_size=Vector2(280,11)
    bar.show_percentage=false
    var background:=StyleBoxFlat.new()
    background.bg_color=Color("26343e")
    background.set_corner_radius_all(3)
    var fill:=StyleBoxFlat.new()
    fill.bg_color=color
    fill.set_corner_radius_all(3)
    bar.add_theme_stylebox_override("background",background)
    bar.add_theme_stylebox_override("fill",fill)
    parent.add_child(bar)
    return bar

func _layout() -> void:
    if not is_instance_valid(root) or not is_instance_valid(modal):return
    var available := get_viewport().get_visible_rect().size
    var card_scale := minf(1.0,minf((available.x-32)/maxf(modal.size.x,1),(available.y-32)/maxf(modal.size.y,1)))
    modal.scale=Vector2.ONE*card_scale
    modal.position=(available-modal.size*card_scale)*.5

func _clear_modal() -> void:
    inventory_labels.clear()
    tactical_map_control=null
    for child in modal_body.get_children():
        modal_body.remove_child(child)
        child.queue_free()

func _title(eyebrow: String, title: String, body: String) -> void:
    modal_body.add_child(_label(eyebrow,12,GOLD))
    modal_body.add_child(_label(title,34))
    var description := _label(body,14,MUTED)
    description.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
    description.custom_minimum_size.x=390
    modal_body.add_child(description)

func _show_panel(screen: String) -> void:
    panel_screen=screen
    _clear_modal()
    if screen=="menu":
        _title("ADAPTIVE SOCIAL SURVIVAL","LAST LIGHT","An Arctic base. Persistent survivors. Restore power and signals, choose your allies and reach either landing.")
        var scenarios := OptionButton.new()
        scenarios.add_item("Peaceful · no distractor bots",0)
        scenarios.add_item("Standard · five active distractors",5)
        scenarios.add_item("Hostile · eight active distractors",8)
        scenarios.select(0 if selected_bots==0 else 2 if selected_bots==8 else 1)
        scenarios.item_selected.connect(func(index:int):selected_bots=scenarios.get_item_id(index))
        modal_body.add_child(scenarios)
        var reset_option:=CheckButton.new()
        reset_option.text="Reset survivor learning for this new game"
        reset_option.button_pressed=reset_new_game
        reset_option.toggled.connect(func(value:bool):reset_new_game=value)
        modal_body.add_child(reset_option)
        modal_body.add_child(_button("START SURVIVAL",_start_mission,true))
        modal_body.add_child(_button("Survivor profiles",func():_show_panel("profiles")))
        var continue_button := _button("Continue saved mission",func():game_command("load"))
        continue_button.disabled=not state.get("save_available",false)
        modal_body.add_child(continue_button)
        modal_body.add_child(_button("Options",func():_show_panel("options")))
        modal_body.add_child(_button("How to play",func():_show_panel("help")))
        modal_body.add_child(_button("Quit",func():game_command("quit")))
    elif screen=="pause":
        _title("MISSION PAUSED","Take a breath","The world is paused. Your inventory and survivor progress are preserved.")
        modal_body.add_child(_button("RESUME",func():game_command("resume"),true))
        modal_body.add_child(_button("Save mission",func():game_command("save")))
        modal_body.add_child(_button("Load last save",func():game_command("load")))
        modal_body.add_child(_button("Inspect survivor profiles",func():_show_panel("profiles")))
        modal_body.add_child(_button("Options",func():_show_panel("options")))
        modal_body.add_child(_button("How to play",func():_show_panel("help")))
        modal_body.add_child(_button("Return to main menu",func():game_command("menu")))
    elif screen=="map":
        _title("TACTICAL MAP","Arctic base","Gold: next mission objective. Blue: supplies you have seen. White: you. Teal: known survivors. Public facilities are labelled.")
        var map_row:=HBoxContainer.new()
        modal_body.add_child(map_row)
        var large_map:=MiniMap.new()
        large_map.custom_minimum_size=Vector2(320,300)
        large_map.set("state",state)
        large_map.set("numbered",true)
        map_row.add_child(large_map)
        tactical_map_control=large_map
        var legend_scroll:=ScrollContainer.new()
        legend_scroll.custom_minimum_size=Vector2(250,300)
        map_row.add_child(legend_scroll)
        var legend:=VBoxContainer.new()
        legend_scroll.add_child(legend)
        var landmark_index:=1
        for landmark in state.get("landmarks",[]):
            legend.add_child(_label("%02d  %s" % [landmark_index,landmark["name"]],12,MUTED))
            landmark_index+=1
        modal_body.add_child(_button("Back to game",func():game_command("map"),true))
    elif screen=="inventory":
        _title("FIELD INVENTORY","Manage the weight you carry","The world continues. Use a supply or discard ONE item permanently to make room. Discard ammo removes up to 12 compatible reserve rounds.")
        var scroll:=ScrollContainer.new()
        scroll.custom_minimum_size=Vector2(560,245)
        modal_body.add_child(scroll)
        var contents:=VBoxContainer.new()
        scroll.add_child(contents)
        for kind in ["medkit","bandage","food","water","fuel","smoke","decoy","ammo"]:
            var count_key:String={"medkit":"medkits","bandage":"bandages","water":"water_bottles","decoy":"decoys","ammo":"reserve"}.get(kind,kind)
            var count:int=int(state.get(count_key,0))
            var row:=HBoxContainer.new()
            contents.add_child(row)
            var description:=_label(kind.capitalize()+" · "+str(count),16,TEXT)
            inventory_labels[kind]=description
            description.custom_minimum_size.x=200
            row.add_child(description)
            if kind not in ["ammo","fuel"]:row.add_child(_button("Use",func():game_command("use_item",{"kind":kind})))
            row.add_child(_button("Discard one" if kind!="ammo" else "Discard ≤12",func():game_command("discard_item",{"kind":kind});_refresh_state();_show_panel("inventory")))
        modal_body.add_child(_button("Back to game",func():game_command("inventory"),true))
    elif screen=="squad":
        _title("SQUAD RADIO","Give intent, keep autonomy","Orders last 45 seconds. Voice needs nearby sight; powered radio works across the base. Survivors can refuse distrustful orders and interrupt for danger, medical care or rescue.")
        for person in state.get("profiles",[]):
            var id:int=person["id"]
            modal_body.add_child(_label(["Maya","Rafi","Lian","Omar"][id-2]+" · "+str(person.get("role","")),16,TEAL))
            var row:=HBoxContainer.new()
            modal_body.add_child(row)
            var labels:PackedStringArray=["Independent","Follow","Hold here","Scavenge","Mission"]
            for order in range(5):
                row.add_child(_button(labels[order],func():game_command("order",{"id":id,"order":order})))
        modal_body.add_child(_button("Back to game",func():game_command("squad"),true))
    elif screen=="profiles" or screen=="inspect":
        _title("SURVIVOR INSPECTOR","Memory shapes strategy","Scores and learned preferences persist between missions until reset. The inspector shows internal AI knowledge for debugging.")
        var selector:=OptionButton.new()
        for person in state.get("profiles",[]):selector.add_item(["Maya","Rafi","Lian","Omar"][int(person["id"])-2],int(person["id"]))
        selector.select(clampi(inspected_id-2,0,3))
        selector.item_selected.connect(func(index:int):inspected_id=selector.get_item_id(index);profile_details.get_v_scroll_bar().value=0;game_command("inspect_actor",{"id":inspected_id});_update_profile_details())
        modal_body.add_child(selector)
        profile_details=RichTextLabel.new()
        profile_details.custom_minimum_size=Vector2(560,245)
        profile_details.bbcode_enabled=true
        profile_details.scroll_active=true
        modal_body.add_child(profile_details)
        modal_body.add_child(_button("Reset this survivor's learned profile",func():game_command("reset_profile",{"id":inspected_id});_refresh_state();_update_profile_details()))
        modal_body.add_child(_button("Back to game" if screen=="inspect" else "Back to pause" if state.get("started",false) else "Back",func():
            if state.get("mode","")=="inspect":game_command("inspect")
            else:_show_panel("pause" if state.get("started",false) else "menu"),true))
        _update_profile_details()
    elif screen=="options":
        _title("OPTIONS","Make it comfortable","Settings are saved locally. The low preset is intended for integrated graphics.")
        modal_body.add_child(_label("Mouse sensitivity",14,MUTED))
        var slider := HSlider.new()
        slider.min_value=.0005;slider.max_value=.006;slider.step=.0001;slider.value=sensitivity
        slider.value_changed.connect(func(value:float):sensitivity=value;_save_options())
        modal_body.add_child(slider)
        var invert := CheckButton.new()
        invert.text="Invert vertical mouse";invert.button_pressed=invert_y
        invert.toggled.connect(func(value:bool):invert_y=value;_save_options())
        modal_body.add_child(invert)
        var quality := OptionButton.new()
        quality.add_item("Low · 960 × 540 · 30 FPS",0)
        quality.add_item("Balanced · 1280 × 720 · 30 FPS",1)
        quality.add_item("Smooth · 1280 × 720 · 60 FPS",2)
        quality.select(resolution)
        quality.item_selected.connect(func(index:int):resolution=index;_save_options();_apply_display())
        modal_body.add_child(quality)
        modal_body.add_child(_label("Sound effects",14,MUTED))
        var volume_slider := HSlider.new()
        volume_slider.min_value=0;volume_slider.max_value=1;volume_slider.step=.05;volume_slider.value=volume
        volume_slider.value_changed.connect(func(value:float):volume=value;_save_options();audio.volume_db=linear_to_db(maxf(volume,.001)))
        modal_body.add_child(volume_slider)
        modal_body.add_child(_button("Back",func():_show_panel("menu" if not state.get("started",false) else "pause"),true))
    elif screen=="help":
        _title("FIELD GUIDE","Stay alive. Find a way.","Find the access key, restore generator power (fuel or hand crank), calibrate any TWO relays, then activate the radio and reach either landing. E opens doors and searches crates. I inspects survivor plans and persistent profiles.")
        var controls := _label("WASD  Move     Mouse  Camera     Shift  Sprint\nCtrl  Crouch     Space  Jump     E  Interact / Revive\nRMB  Aim     LMB  Attack     1 / 2  Pistol / Sword\nR  Reload     H  Medkit     F  Food     G  Offer ammo\nF5  Save     F9  Load     Tab  Spectate\n3/4/5  SMG / Rifle / Shotgun   B  Bandage   T  Drink\nQ  Smoke   V  Decoy   Alt  Roll   C  Door   Y  Team up\nI  Inspector   M  Map   F3  Squad orders   J  Inventory\nF2  Tactical view / wheel zoom   Z  Cover\nF10  Watch the AI play     F1  Debug HUD",14,TEXT)
        modal_body.add_child(controls)
        modal_body.add_child(_button("Back",func():_show_panel("menu" if not state.get("started",false) else "pause"),true))
    elif screen=="results":
        var success: bool=state.get("victory",false)
        var elapsed := int(state.get("elapsed",0))
        _title("MISSION COMPLETE" if success else "MISSION ENDED","SURVIVORS EXTRACTED" if success else "THE SIGNAL WENT DARK", "%d of 5 survivors reached safety.\nTime %02d:%02d  ·  Revives %d  ·  Supplies collected %d" % [state.get("arrived",0),elapsed/60,elapsed%60,state.get("revives",0),state.get("pickups",0)])
        for actor in state.get("team",[]):
            modal_body.add_child(_label(str(actor["name"])+"  ·  "+str(actor["state"]),14,TEAL if actor["state"]=="Extracted" else MUTED))
        modal_body.add_child(_button("PLAY AGAIN",func():game_command("restart"),true))
        modal_body.add_child(_button("Quit",func():game_command("quit")))
    modal_status=_label("",12,TEAL)
    modal_status.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
    modal_status.custom_minimum_size.x=390
    modal_body.add_child(modal_status)
    modal.reset_size()
    call_deferred("_layout")
    var button := modal_body.find_children("*","Button",true,false)
    if not button.is_empty():button[0].grab_focus()

func _refresh_state() -> void:
    state=get_game_state()
    if state.is_empty():return
    var mode: String=state.get("mode","playing")
    if mode!=visible_mode:
        visible_mode=mode
        if mode!="playing":_show_panel(mode)
    modal.visible=mode!="playing"
    shade.visible=modal.visible
    hud_root.visible=mode=="playing"
    var objectives := ["Find the Arctic access key","Power / Two relays / Radio","Reach either landing zone"]
    objective_label.text=objectives[clampi(int(state.get("mission",0)),0,2)]
    var seconds := maxi(0,int(state.get("deadline",600))-int(state.get("elapsed",0)))
    objective_detail.text="%d m · %02d:%02d · Power %s · Relays %d/2" % [int(state.get("objective_distance",0)),seconds/60,seconds%60,"ON" if state.get("power",false) else "OFF",state.get("relays",0)]
    if state.get("compound",false) and int(state.get("mission",0))==2:
        var sites:Array=state.get("landings",[])
        var text:=PackedStringArray()
        for i in range(sites.size()):
            var site:Dictionary=sites[i]
            text.append(("North" if i==0 else "West")+": "+("CONTESTED" if site.get("contested",false) else "%.0fs" % site.get("remaining",0) if site.get("active",false) else "Activate beacon"))
        objective_detail.text=" · ".join(text)
    stat_label.text="HP %d · Cold %d · Water %d · Armour %d\nLoad %.1f/%.0f · Med %d · Smoke %d" % [state.get("health",100),state.get("cold",0),state.get("hydration",100),state.get("armour",0),state.get("weight",0),state.get("capacity",14),state.get("medkits",0),state.get("smoke",0)]
    for kind in inventory_labels:
        var label:Label=inventory_labels[kind]
        var key:String={"medkit":"medkits","bandage":"bandages","water":"water_bottles","decoy":"decoys","ammo":"reserve"}.get(kind,kind)
        if is_instance_valid(label):label.text=str(kind).capitalize()+" · "+str(state.get(key,0))
    health_bar.value=state.get("health",100)
    stamina_bar.value=state.get("stamina",100)
    hunger_bar.value=state.get("hunger",100)
    var player_status: String=state.get("player_state","Idle")
    tutorial_tip.text="You reached safety  ·  Tab to spectate the remaining survivors" if player_status=="Extracted" else "Downed  ·  A teammate can revive you  ·  Tab to spectate" if player_status=="Downed" else "You died  ·  Tab to spectate  ·  Esc for menu" if player_status=="Dead" else "WASD · E Interact · F2 View · Z Cover · J Inventory · F3 Squad · I Profiles · M Map"
    var weapon: String=state.get("weapon","UNARMED")
    crosshair.visible=not state.get("overview",false)
    crosshair.text="×" if state.get("hit_confirm",false) else "·"
    crosshair.modulate=GOLD if state.get("hit_confirm",false) else Color.WHITE
    ammo_label.text="%02d / %02d" % [state.get("rounds",0),state.get("reserve",0)] if weapon in ["PISTOL","SMG","RIFLE","SHOTGUN"] else weapon
    action_label.text=weapon+" · "+str(state.get("player_state","Idle"))
    if float(state.get("action_timer",0))>0:action_label.text+="  %.1fs" % state["action_timer"]
    prompt_label.text=str(state.get("hint",""))
    prompt_label.visible=not prompt_label.text.is_empty()
    toast_label.text=str(state.get("message",""))
    toast_label.visible=mode=="playing"
    if is_instance_valid(modal_status):
        modal_status.text=toast_label.text
        modal_status.visible=not modal_status.text.is_empty() and not modal_status.text.begins_with("Last Light ready")
    toast_label.offset_top=115 if mode=="playing" else get_viewport().get_visible_rect().size.y-60
    toast_label.offset_bottom=toast_label.offset_top+28
    for i in range(4):
        var actors: Array=state.get("team",[])
        if actors.size()<=i+1:continue
        var actor: Dictionary=actors[i+1]
        squad_rows[i].text="%s  ·  %s" % [actor["name"],str(actor["health"])+" HP" if actor["known"] else "No contact"]
        if actor["state"]=="Downed":squad_rows[i].text+="  /  NEEDS HELP"
        if actor["state"]=="Extracted":squad_rows[i].text=str(actor["name"])+"  /  SAFE"
        squad_rows[i].add_theme_color_override("font_color",TEAL if actor["state"]=="Extracted" else TEXT)
    map_control.set("state",state)
    map_control.queue_redraw()
    if is_instance_valid(tactical_map_control):
        tactical_map_control.set("state",state)
        tactical_map_control.queue_redraw()
    debug_label.visible=state.get("debug",false)
    debug_label.text="FPS %d   Draws %d   Nav %d   Python %s\nWASD / Shift / Ctrl / Space  ·  F10 autoplay %s" % [state.get("fps",0),state.get("draw_calls",0),state.get("navigation_polygons",0),"connected" if state.get("python",false) else "authored",state.get("autoplay",false)]
    var health := int(state.get("health",100))
    if health<last_player_health:
        flash.color.a=.25
        _play_sound("hurt")
    last_player_health=health
    for event in state.get("events",[]):
        var sequence := int(event.get("sequence",0))
        if sequence<=last_sound_event:continue
        last_sound_event=sequence
        var kind: String=event.get("kind","")
        if kind=="pickup" and event.get("actor",0)==1:_play_sound("pickup")
        if kind=="radio":_play_sound("radio")
        if kind=="help":_play_sound("gate")
        if kind in ["attack","shot"] and event.get("actor",0)==1:_play_sound("shot")

func _play_sound(name: String) -> void:
    if sounds.has(name) and is_instance_valid(audio):
        audio.stream=sounds[name]
        audio.play()

func _process(delta: float) -> void:
    if not is_instance_valid(root):return
    refresh+=delta
    profile_refresh+=delta
    if profile_refresh>=.5:
        profile_refresh=0
        if panel_screen in ["inspect","profiles"] and modal.visible:_update_profile_details()
    if refresh>=.1:
        refresh=0
        _refresh_state()
    if is_instance_valid(flash):flash.color.a=move_toward(flash.color.a,0,delta*1.2)

func _load_options() -> void:
    var file := ConfigFile.new()
    if file.load("user://settings.cfg")==OK:
        sensitivity=_safe_number(file.get_value("input","sensitivity",.0025),.0025,.0005,.006)
        var inversion: Variant=file.get_value("input","invert_y",false)
        invert_y=inversion if inversion is bool else false
        resolution=int(_safe_number(file.get_value("display","preset",0),0,0,2))
        volume=_safe_number(file.get_value("sound","volume",.6),.6,0,1)
    apply_options({"sensitivity":sensitivity,"invert_y":invert_y,"volume":volume})
    if DisplayServer.get_name()!="headless":_apply_display()

func _safe_number(value: Variant, fallback: float, low: float, high: float) -> float:
    if not (value is float or value is int):return fallback
    if not is_finite(float(value)):return fallback
    return clampf(float(value),low,high)

func _save_options() -> void:
    var file := ConfigFile.new()
    file.set_value("input","sensitivity",sensitivity)
    file.set_value("input","invert_y",invert_y)
    file.set_value("display","preset",resolution)
    file.set_value("sound","volume",volume)
    file.save("user://settings.cfg")
    apply_options({"sensitivity":sensitivity,"invert_y":invert_y,"volume":volume})

func _apply_display() -> void:
    Engine.max_fps=60 if resolution==2 else 30
    if DisplayServer.get_name()!="headless":
        DisplayServer.window_set_size(Vector2i(960,540) if resolution==0 else Vector2i(1280,720))

func _start_mission() -> void:
    if reset_new_game:game_command("reset_all_profiles")
    game_command("start",{"bots":selected_bots})

func _update_profile_details() -> void:
    if not is_instance_valid(profile_details):return
    for person in state.get("profiles",[]):
        if int(person["id"])!=inspected_id:continue
        var lines:=PackedStringArray()
        lines.append("[b]"+str(person["type"])+"[/b] · Reset revision "+str(person["revision"]))
        lines.append("Experiences %d · Help received %d · Harm received %d · Help given %d · Extracted %d · Deaths %d" % [person["experiences"],person["help_received"],person["harm_received"],person["help_given"],person["extractions"],person["deaths"]])
        for trait_name in person.get("traits",{}):lines.append(str(trait_name).capitalize()+": %.1f / 100" % person["traits"][trait_name])
        lines.append("Health %d · Hunger %.0f · Water %.0f · Cold %.0f · %s %d/%d · Teammate %d" % [person.get("health",0),person.get("hunger",0),person.get("hydration",0),person.get("cold",0),person.get("weapon",""),person.get("rounds",0),person.get("reserve",0),person.get("teammate",0)])
        lines.append("[b]Role[/b]: %s · Suppression %.0f%% · Context %d · Learned situations %d" % [person.get("role",""),float(person.get("suppression",0))*100,person.get("context",0),person.get("learned_contexts",0)])
        lines.append(str(person.get("order_reason","")))
        lines.append("[b]Chosen plan[/b]: "+str(person.get("goal","No active mission")))
        lines.append("Current action: "+str(person.get("reason","")))
        lines.append("Search expanded %d candidates · Base neural guide: %s" % [person.get("expanded",0),"connected" if person.get("neural",false) else "offline / planner fallback"])
        lines.append("Learned intent biases [Explore, Acquire, Cooperate, Avoid, Defend, Progress]: "+str(person.get("preferences",[])))
        for step in person.get("steps",[]):lines.append("  → "+str(step))
        lines.append("Route budget: %s · Expected HP %.0f · Compatible ammo %.0f" % ["complete" if person.get("plan_complete",false) else "partial / unresolved",person.get("expected_health",0),person.get("expected_ammo",0)])
        lines.append("[b]Tactical choices[/b] (utility, not probability): "+str(person.get("tactic","")))
        for learned in person.get("contextual_values",[]):
            if int(learned.get("context",-1))==int(person.get("context",-2)):lines.append("Learned values for this situation: "+str(learned.get("values",[])))
        for option in person.get("tactics",[]):
            lines.append(("%.1f" % option.get("score",0) if option.get("legal",false) else "VETO")+" · "+str(option.get("name",""))+" · "+str(option.get("reason","")))
        lines.append("[b]Private observations[/b]")
        for contact in person.get("contacts",[]):
            lines.append("Actor %d · %.1fs old · %.0f%% confidence · %s · %s" % [contact["id"],contact["age"],float(contact["confidence"])*100,"visible" if contact["visible"] else "last known","hostile" if contact["hostile"] else "neutral / ally"])
        lines.append("[b]Alternative plans[/b]")
        for plan in person.get("alternatives",[]):lines.append("%.1f score · %.1f cost · %.2f risk · %s" % [plan["score"],plan["cost"],plan["risk"],plan["reason"]])
        lines.append("[b]Historical region risk[/b] (0..100)")
        var region_index:=0
        for risk in person.get("region_risk",[]):
            if float(risk)>.001:
                var landmarks:Array=state.get("landmarks",[])
                var region_name:String=landmarks[region_index]["name"] if region_index<landmarks.size() else str(region_index)
                lines.append(region_name+": %.1f" % (float(risk)*100))
            region_index+=1
        lines.append("[b]Persistent relationships[/b]")
        for peer in person.get("relationships",[]):lines.append("Actor %d · Trust %.0f · Fear %.0f" % [peer["id"],peer["trust"],peer["fear"]])
        lines.append("[b]Recent evidence[/b]")
        for event in person.get("history",[]):lines.append(str(event.get("kind",""))+" · Actor "+str(event.get("peer",0))+" · Reward "+str(event.get("reward",0)))
        var scroll:=profile_details.get_v_scroll_bar().value
        profile_details.text="\n".join(lines)
        profile_details.get_v_scroll_bar().set_deferred("value",scroll)
        return
