extends Control
var state: Dictionary = {}
var numbered := false
var cached_rows := ""
var terrain: Texture2D
func _draw() -> void:
    var rows: Array=state.get("map_rows",[])
    if rows.is_empty():return
    var width:=str(rows[0]).length()
    var height:=rows.size()
    var signature:=str(width)+":"+str(height)+":"+str(state.get("arctic",false))
    if signature!=cached_rows:
        cached_rows=signature
        var image:=Image.create(width,height,false,Image.FORMAT_RGBA8)
        for z in range(height):
            for x in range(width):image.set_pixel(x,z,Color("1d3b4e") if str(rows[z])[x]=="#" else Color("6b8896") if state.get("arctic",false) else Color("17232c"))
        terrain=ImageTexture.create_from_image(image)
    var cell:=minf(size.x/width,size.y/height)
    var offset:=(size-Vector2(width,height)*cell)*.5
    draw_texture_rect(terrain,Rect2(offset,Vector2(width,height)*cell),false)
    if state.get("compound",false):
        for road in [Rect2(119,55,30,155),Rect2(75,177,145,16),Rect2(135,80,65,20)]:
            draw_rect(Rect2(offset+road.position/2*cell,road.size/2*cell),Color("46535b"))
        for building in state.get("structures",[]):
            var footprint:=Rect2(Vector2(building["position"])-Vector2(building["size"])*.5,Vector2(building["size"]))
            draw_rect(Rect2(offset+footprint.position/2*cell,footprint.size/2*cell),Color("94a8ac"))
    for contact in state.get("contacts",[]):
        draw_circle(offset+(Vector2(contact["position"])/2+Vector2.ONE*.5)*cell,3,Color(1,.35,.25,float(contact["confidence"])) if contact["hostile"] else Color(.85,.7,.3,.7))
    var route:PackedVector2Array=[]
    for point in state.get("route",[]):route.append(offset+(Vector2(point)/2+Vector2.ONE*.5)*cell)
    if route.size()>1:draw_polyline(route,Color("69c2b5"),1)
    var landmark_index:=1
    for landmark in state.get("landmarks",[]):
        var location:=offset+(Vector2(landmark["position"])/2+Vector2.ONE*.5)*cell
        draw_rect(Rect2(location-Vector2(2,2),Vector2(4,4)),Color("b4d7e1"))
        if size.x>300:draw_string(ThemeDB.fallback_font,location+Vector2(4,-4),str(landmark_index) if numbered else str(landmark["name"]),HORIZONTAL_ALIGNMENT_LEFT,-1,12,Color("e1edf0"))
        landmark_index+=1
    var objective:Vector2=state.get("objective_position",Vector2.ZERO)
    draw_circle(offset+(objective/2+Vector2.ONE*.5)*cell,4,Color("f5be64"))
    for item in state.get("items",[]):draw_circle(offset+(Vector2(item["position"])/2+Vector2.ONE*.5)*cell,2,Color("6cd2e0"))
    for actor in state.get("team",[]):
        if not actor.get("known",false):continue
        var p:Vector2=actor["position"]
        draw_circle(offset+(p/2+Vector2.ONE*.5)*cell,3,Color("f1f6f7") if actor["id"]==1 else Color("69c2b5") if actor.get("health",100)>0 else Color("e17474"))
