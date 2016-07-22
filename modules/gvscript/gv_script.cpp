#include "gv_script.h"

#include "core/global_constants.h"
#include "core/globals.h"
#include "os/file_access.h"
#include "io/file_access_encrypted.h"
#include "gv_functions.h"

GVInstance *GVScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_isref, Variant::CallError &r_error) {
	/* STEP 1, CREATE */

	GVInstance* instance = memnew( GVInstance );
	instance->base_ref=p_isref;
	instance->members.resize(member_indices.size());
	instance->script=Ref<GVScript>(this);
	instance->owner=p_owner;
#ifdef DEBUG_ENABLED
	//needed for hot reloading
	for(Map<StringName,MemberInfo>::Element *E=member_indices.front();E;E=E->next()) {
		instance->member_indices_cache[E->key()]=E->get().index;
	}
#endif
	instance->owner->set_script_instance(instance);

	/* STEP 2, INITIALIZE AND CONSRTUCT */

	instances.insert(instance->owner);

	initializer->call(instance,p_args,p_argcount,r_error);

	if (r_error.error!=Variant::CallError::CALL_OK) {
		instance->script=Ref<GVScript>();
		instance->owner->set_script_instance(NULL);
		instances.erase(p_owner);
		ERR_FAIL_COND_V(r_error.error!=Variant::CallError::CALL_OK, NULL); //error constructing
	}

	//@TODO make thread safe
	return instance;
}

void GVScript::_set_subclass_path(Ref<GVScript> &p_sc, const String &p_path) {
	p_sc->path=p_path;
	for(Map<StringName,Ref<GVScript> >::Element *E=p_sc->subclasses.front();E;E=E->next()) {

		_set_subclass_path(E->get(),p_path);
	}
}

bool GVScript::_update_exports() {

//#ifdef TOOLS_ENABLED  NOTE::MARIANOGNU: UNCOMENT

// TODO::MARIANOGNU: fill this function after export method is defined

//#endif   NOTE::MARIANOGNU: UNCOMENT
	return false;
}

bool GVScript::_get(const StringName &p_name, Variant &r_ret) const {

	const GVScript *top=this;
	while(top) {

		{
			const Map<StringName,Variant>::Element *E=top->constants.find(p_name);
			if (E) {

				r_ret= E->get();
				return true;
			}
		}

		{
			const Map<StringName,Ref<GVScript> >::Element *E=subclasses.find(p_name);
			if (E) {

				r_ret=E->get();
				return true;
			}
		}
		top=top->_base;
	}

	// TODO::MARIANOGNU: get the script nodes and connections

	return false;
}

bool GVScript::_set(const StringName &p_name, const Variant &p_value) {
	//TODO::MARIANOGNU: set the script nodes and connections
}

void GVScript::_get_property_list(List<PropertyInfo> *p_properties) const {
	//TODO::MARIANOGNU: fill the property list with node and connections info
}

Variant GVScript::call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	GVScript *top=this;
	while(top) {

		Map<StringName,GVFunction*>::Element *E=top->member_functions.find(p_method);
		if (E) {

			if (!E->get()->is_static()) {
				WARN_PRINT(String("Can't call non-static function: '"+String(p_method)+"' in script.").utf8().get_data());
			}

			return E->get()->call(NULL,p_args,p_argcount,r_error);
		}
		top=top->_base;
	}

	//none found, regular

	return Script::call(p_method,p_args,p_argcount,r_error);
}

void GVScript::_bind_methods() {
	ObjectTypeDB::bind_native_method(METHOD_FLAGS_DEFAULT,"new",&GVScript::_new,MethodInfo("new"));
}

GVScript::GVScript() {
}


//////////////////////////////
//         INSTANCE         //
//////////////////////////////


bool GVInstance::set(const StringName& p_name, const Variant& p_value) {

	//member
	{
		const Map<StringName,GVScript::MemberInfo>::Element *E = script->member_indices.find(p_name);
		if (E) {
			if (E->get().setter) {
				const Variant *val=&p_value;
				Variant::CallError err;
				call(E->get().setter,&val,1,err);
				if (err.error==Variant::CallError::CALL_OK) {
					return true; //function exists, call was successful
				}
			}
			else
				members[E->get().index] = p_value;
			return true;
		}
	}

	GVScript *sptr=script.ptr();
	while(sptr) {


		Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(GVScriptLanguage::get_singleton()->strings._set);
		if (E) {

			Variant name=p_name;
			const Variant *args[2]={&name,&p_value};

			Variant::CallError err;
			Variant ret = E->get()->call(this,(const Variant**)args,2,err);
			if (err.error==Variant::CallError::CALL_OK && ret.get_type()==Variant::BOOL && ret.operator bool())
				return true;
		}
		sptr = sptr->_base;
	}

	return false;
}

bool GVInstance::get(const StringName& p_name, Variant &r_ret) const {



	const GVScript *sptr=script.ptr();
	while(sptr) {

		{
			const Map<StringName,GVScript::MemberInfo>::Element *E = script->member_indices.find(p_name);
			if (E) {
				if (E->get().getter) {
					Variant::CallError err;
					r_ret=const_cast<GVInstance*>(this)->call(E->get().getter,NULL,0,err);
					if (err.error==Variant::CallError::CALL_OK) {
						return true;
					}
				}
				r_ret=members[E->get().index];
				return true; //index found

			}
		}

		{

			const GVScript *sl = sptr;
			while(sl) {
				const Map<StringName,Variant>::Element *E = sl->constants.find(p_name);
				if (E) {
					r_ret=E->get();
					return true; //index found

				}
				sl=sl->_base;
			}
		}

		{
			const Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(GVScriptLanguage::get_singleton()->strings._get);
			if (E) {

				Variant name=p_name;
				const Variant *args[1]={&name};

				Variant::CallError err;
				Variant ret = const_cast<GVFunction*>(E->get())->call(const_cast<GVInstance*>(this),(const Variant**)args,1,err);
				if (err.error==Variant::CallError::CALL_OK && ret.get_type()!=Variant::NIL) {
					r_ret=ret;
					return true;
				}
			}
		}
		sptr = sptr->_base;
	}

	return false;

}

Variant::Type GVInstance::get_property_type(const StringName& p_name,bool *r_is_valid) const {


	const GVScript *sptr=script.ptr();
	while(sptr) {

		if (sptr->member_info.has(p_name)) {
			if (r_is_valid)
				*r_is_valid=true;
			return sptr->member_info[p_name].type;
		}
		sptr = sptr->_base;
	}

	if (r_is_valid)
		*r_is_valid=false;
	return Variant::NIL;
}

void GVInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	// exported members, not done yet!

	const GVScript *sptr=script.ptr();
	List<PropertyInfo> props;

	while(sptr) {


		const Map<StringName,GDFunction*>::Element *E = sptr->member_functions.find(GVScriptLanguage::get_singleton()->strings._get_property_list);
		if (E) {


			Variant::CallError err;
			Variant ret = const_cast<GVFunction*>(E->get())->call(const_cast<GVInstance*>(this),NULL,0,err);
			if (err.error==Variant::CallError::CALL_OK) {

				if (ret.get_type()!=Variant::ARRAY) {

					ERR_EXPLAIN("Wrong type for _get_property list, must be an array of dictionaries.");
					ERR_FAIL();
				}
				Array arr = ret;
				for(int i=0;i<arr.size();i++) {

					Dictionary d = arr[i];
					ERR_CONTINUE(!d.has("name"));
					ERR_CONTINUE(!d.has("type"));
					PropertyInfo pinfo;
					pinfo.type = Variant::Type( d["type"].operator int());
					ERR_CONTINUE(pinfo.type<0 || pinfo.type>=Variant::VARIANT_MAX );
					pinfo.name = d["name"];
					ERR_CONTINUE(pinfo.name=="");
					if (d.has("hint"))
						pinfo.hint=PropertyHint(d["hint"].operator int());
					if (d.has("hint_string"))
						pinfo.hint_string=d["hint_string"];
					if (d.has("usage"))
						pinfo.usage=d["usage"];

					props.push_back(pinfo);

				}

			}
		}

		//instance a fake script for editing the values

		Vector<_GVScriptMemberSort> msort;
		for(Map<StringName,PropertyInfo>::Element *E=sptr->member_info.front();E;E=E->next()) {

			_GVScriptMemberSort ms;
			ERR_CONTINUE(!sptr->member_indices.has(E->key()));
			ms.index=sptr->member_indices[E->key()].index;
			ms.name=E->key();
			msort.push_back(ms);

		}

		msort.sort();
		msort.invert();
		for(int i=0;i<msort.size();i++) {
			props.push_front(sptr->member_info[msort[i].name]);
		}

		sptr = sptr->_base;
	}

	//props.invert();

	for (List<PropertyInfo>::Element *E=props.front();E;E=E->next()) {
		p_properties->push_back(E->get());
	}
}

void GVInstance::get_method_list(List<MethodInfo> *p_list) const {

	const GVScript *sptr=script.ptr();
	while(sptr) {

		for (Map<StringName,GVFunction*>::Element *E = sptr->member_functions.front();E;E=E->next()) {

			MethodInfo mi;
			mi.name=E->key();
			mi.flags|=METHOD_FLAG_FROM_SCRIPT;
			for(int i=0;i<E->get()->get_argument_count();i++)
				mi.arguments.push_back(PropertyInfo(Variant::NIL,"arg"+itos(i)));
			p_list->push_back(mi);
		}
		sptr = sptr->_base;
	}

}

bool GVInstance::has_method(const StringName& p_method) const {

	const GVScript *sptr=script.ptr();
	while(sptr) {
		const Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(p_method);
		if (E)
			return true;
		sptr = sptr->_base;
	}

	return false;
}
Variant GVInstance::call(const StringName& p_method,const Variant** p_args,int p_argcount,Variant::CallError &r_error) {

	//printf("calling %ls:%i method %ls\n", script->get_path().c_str(), -1, String(p_method).c_str());

	GVScript *sptr=script.ptr();
	while(sptr) {
		Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(p_method);
		if (E) {
			return E->get()->call(this,p_args,p_argcount,r_error);
		}
		sptr = sptr->_base;
	}
	r_error.error=Variant::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void GVInstance::call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount) {

	GVScript *sptr=script.ptr();
	Variant::CallError ce;

	while(sptr) {
		Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(p_method);
		if (E) {
			E->get()->call(this,p_args,p_argcount,ce);
		}
		sptr = sptr->_base;
	}

}


void GVInstance::_ml_call_reversed(GVScript *sptr,const StringName& p_method,const Variant** p_args,int p_argcount) {

	if (sptr->_base)
		_ml_call_reversed(sptr->_base,p_method,p_args,p_argcount);

	Variant::CallError ce;

	Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(p_method);
	if (E) {
		E->get()->call(this,p_args,p_argcount,ce);
	}

}

void GVInstance::call_multilevel_reversed(const StringName& p_method,const Variant** p_args,int p_argcount) {

	if (script.ptr()) {
		_ml_call_reversed(script.ptr(),p_method,p_args,p_argcount);
	}
}

void GVInstance::notification(int p_notification) {

	//notification is not virutal, it gets called at ALL levels just like in C.
	Variant value=p_notification;
	const Variant *args[1]={&value };

	GVScript *sptr=script.ptr();
	while(sptr) {
		Map<StringName,GVFunction*>::Element *E = sptr->member_functions.find(GVScriptLanguage::get_singleton()->strings._notification);
		if (E) {
			Variant::CallError err;
			E->get()->call(this,args,1,err);
			if (err.error!=Variant::CallError::CALL_OK) {
				//print error about notification call

			}
		}
		sptr = sptr->_base;
	}

}

Ref<Script> GVInstance::get_script() const {

	return script;
}

ScriptLanguage *GVInstance::get_language() {

	return GVScriptLanguage::get_singleton();
}

void GVInstance::reload_members() {

#ifdef DEBUG_ENABLED

	members.resize(script->member_indices.size()); //resize

	Vector<Variant> new_members;
	new_members.resize(script->member_indices.size());

	//pass the values to the new indices
	for(Map<StringName,GVScript::MemberInfo>::Element *E=script->member_indices.front();E;E=E->next()) {

		if (member_indices_cache.has(E->key())) {
			Variant value = members[member_indices_cache[E->key()]];
			new_members[E->get().index]=value;
		}

	}

	//apply
	members=new_members;

	//pass the values to the new indices
	member_indices_cache.clear();
	for(Map<StringName,GVScript::MemberInfo>::Element *E=script->member_indices.front();E;E=E->next()) {

		member_indices_cache[E->key()]=E->get().index;
	}

#endif
}

GVInstance::GVInstance() {
	owner=NULL;
	base_ref=false;
}

GVInstance::~GVInstance() {
	if (script.is_valid() && owner) {
		script->instances.erase(owner);
	}
}


bool GVNativeClass::_get(const StringName &p_name, Variant &r_ret) const {
	bool ok;
	int v = ObjectTypeDB::get_integer_constant(name, p_name, &ok);

	if (ok) {
		r_ret=v;
		return true;
	} else {
		return false;
	}
}

void GVNativeClass::_bind_methods() {
	ObjectTypeDB::bind_method(_MD("new"),&GVNativeClass::_new);
}

Variant GVNativeClass::_new() {
	Object *o = instance();
	if (!o) {
		ERR_EXPLAIN("Class type: '"+String(name)+"' is not instantiable.");
		ERR_FAIL_COND_V(!o,Variant());
	}

	Reference *ref = o->cast_to<Reference>();
	if (ref) {
		return REF(ref);
	} else {
		return o;
	}
}

Object *GVNativeClass::instance() {
	return ObjectTypeDB::instance(name);
}

GVNativeClass::GVNativeClass(const StringName &p_name) {
	name=p_name;
}


void GVScriptLanguage::_add_global(const StringName &p_name, const Variant &p_value) {
	if (globals.has(p_name)) {
		//overwrite existing
		global_array[globals[p_name]]=p_value;
		return;
	}
	globals[p_name]=global_array.size();
	global_array.push_back(p_value);
	_global_array=global_array.ptr();
}

void GVScriptLanguage::init() {
	//populate global constants
	int gcc=GlobalConstants::get_global_constant_count();
	for(int i=0;i<gcc;i++) {

		_add_global(StaticCString::create(GlobalConstants::get_global_constant_name(i)),GlobalConstants::get_global_constant_value(i));
	}

	_add_global(StaticCString::create("PI"),Math_PI);

	//populate native classes

	List<StringName> class_list;
	ObjectTypeDB::get_type_list(&class_list);
	for(List<StringName>::Element *E=class_list.front();E;E=E->next()) {

		StringName n = E->get();
		String s = String(n);
		if (s.begins_with("_"))
			n=s.substr(1,s.length());

		if (globals.has(n))
			continue;
		Ref<GVNativeClass> nc = memnew( GVNativeClass(E->get()) );
		_add_global(n,nc);
	}

	//populate singletons

	List<Globals::Singleton> singletons;
	Globals::get_singleton()->get_singletons(&singletons);
	for(List<Globals::Singleton>::Element *E=singletons.front();E;E=E->next()) {

		_add_global(E->get().name,E->get().ptr);
	}
}

String GVScriptLanguage::get_type() const {
	return "GVScript";
}

String GVScriptLanguage::get_extension() const {
	return "gv";
}

Error GVScriptLanguage::execute_file(const String &p_path) {
	// ??
	return OK;
}

void GVScriptLanguage::finish() {

}

String GVScriptLanguage::get_template(const String &p_class_name, const String &p_base_class_name) const {
	//NOTE::MARIANOGNU: need a way to ocnvert String into nodes and connections, probably using _set
	return String();
}

void GVScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
	_add_global(p_variable,p_value);
}

struct GVScriptDepSort {

	//must support sorting so inheritance works properly (parent must be reloaded first)
	bool operator()(const Ref<GVScript> &A, const Ref<GVScript>& B) const {

		if (A==B)
			return false; //shouldn't happen but..
		const GVScript *I=B->get_base().ptr();
		while(I) {
			if (I==A.ptr()) {
				// A is a base of B
				return true;
			}

			I=I->get_base().ptr();
		}

		return false; //not a base
	}
};

void GVScriptLanguage::reload_all_scripts()
{
#ifdef DEBUG_ENABLED
	print_line("RELOAD ALL SCRIPTS");
	if (lock) {
		lock->lock();
	}

	List<Ref<GVScript> > scripts;

	SelfList<GVScript> *elem=script_list.first();
	while(elem) {
		if (elem->self()->get_path().is_resource_file()) {
			print_line("FOUND: "+elem->self()->get_path());
			scripts.push_back(Ref<GVScript>(elem->self())); //cast to gvscript to avoid being erased by accident
		}
		elem=elem->next();
	}

	if (lock) {
		lock->unlock();
	}

	//as scripts are going to be reloaded, must proceed without locking here

	scripts.sort_custom<GVScriptDepSort>(); //update in inheritance dependency order

	for(List<Ref<GVScript> >::Element *E=scripts.front();E;E=E->next()) {

		print_line("RELOADING: "+E->get()->get_path());
		E->get()->load_source_code(E->get()->get_path());
		E->get()->reload(true);
	}
#endif
}

void GVScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	//NOTE::MARIANOGNU: since GVScript aims to be static typed this migth need to be changed, but since we are using
	//					the Variant API this might not be needed...
#ifdef DEBUG_ENABLED

	if (lock) {
		lock->lock();
	}

	List<Ref<GVScript> > scripts;

	SelfList<GVScript> *elem=script_list.first();
	while(elem) {
		if (elem->self()->get_path().is_resource_file()) {

			scripts.push_back(Ref<GVScript>(elem->self())); //cast to gvscript to avoid being erased by accident
		}
		elem=elem->next();
	}

	if (lock) {
		lock->unlock();
	}

	//when someone asks you why dynamically typed languages are easier to write....

	Map< Ref<GVScript>, Map<ObjectID,List<Pair<StringName,Variant> > > > to_reload;

	//as scripts are going to be reloaded, must proceed without locking here

	scripts.sort_custom<GVScriptDepSort>(); //update in inheritance dependency order

	for(List<Ref<GVScript> >::Element *E=scripts.front();E;E=E->next()) {

		bool reload = E->get()==p_script || to_reload.has(E->get()->get_base());

		if (!reload)
			continue;

		to_reload.insert(E->get(),Map<ObjectID,List<Pair<StringName,Variant> > >());

		if (!p_soft_reload) {

			//save state and remove script from instances
			Map<ObjectID,List<Pair<StringName,Variant> > >& map = to_reload[E->get()];

			while(E->get()->instances.front()) {
				Object *obj = E->get()->instances.front()->get();
				//save instance info
				List<Pair<StringName,Variant> > state;
				if (obj->get_script_instance()) {

					obj->get_script_instance()->get_property_state(state);
					map[obj->get_instance_ID()]=state;
					obj->set_script(RefPtr());
				}
			}

			//same thing for placeholders
#ifdef TOOLS_ENABLED

			while(E->get()->placeholders.size()) {

				Object *obj = E->get()->placeholders.front()->get()->get_owner();
				//save instance info
				List<Pair<StringName,Variant> > state;
				if (obj->get_script_instance()) {

					obj->get_script_instance()->get_property_state(state);
					map[obj->get_instance_ID()]=state;
					obj->set_script(RefPtr());
				}
			}
#endif

		}
	}

	for(Map< Ref<GVScript>, Map<ObjectID,List<Pair<StringName,Variant> > > >::Element *E=to_reload.front();E;E=E->next()) {

		Ref<GVScript> scr = E->key();
		scr->reload(true);

		//restore state if saved
		for (Map<ObjectID,List<Pair<StringName,Variant> > >::Element *F=E->get().front();F;F=F->next()) {

			Object *obj = ObjectDB::get_instance(F->key());
			if (!obj)
				continue;

			obj->set_script(scr.get_ref_ptr());
			if (!obj->get_script_instance())
				continue;

			for (List<Pair<StringName,Variant> >::Element *G=F->get().front();G;G=G->next()) {
				obj->get_script_instance()->set(G->get().first,G->get().second);
			}
		}

		//if instance states were saved, set them!
	}


#endif
}

void GVScriptLanguage::frame() {
	calls=0;

#ifdef DEBUG_ENABLED
	if (profiling) {
		if (lock) {
			lock->lock();
		}

		SelfList<GVFunction> *elem=function_list.first();
		while(elem) {
			elem->self()->profile.last_frame_call_count=elem->self()->profile.frame_call_count;
			elem->self()->profile.last_frame_self_time=elem->self()->profile.frame_self_time;
			elem->self()->profile.last_frame_total_time=elem->self()->profile.frame_total_time;
			elem->self()->profile.frame_call_count=0;
			elem->self()->profile.frame_self_time=0;
			elem->self()->profile.frame_total_time=0;
			elem=elem->next();
		}


		if (lock) {
			lock->unlock();
		}
	}

#endif
}

void GVScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
	for(int i=0;i<GVFunctions::FUNC_MAX;i++) {

		p_functions->push_back(GVFunctions::get_info(GVFunctions::Function(i)));
	}

	//not really "functions", but..
	//NOTE::MARIANOGNU: this might be handled other way, since resources might be subresources of the script
	{
		MethodInfo mi;
		mi.name="preload:Resource";
		mi.arguments.push_back(PropertyInfo(Variant::STRING,"path"));
		mi.return_val=PropertyInfo(Variant::OBJECT,"",PROPERTY_HINT_RESOURCE_TYPE,"Resource");
		p_functions->push_back(mi);
	}
	{
		MethodInfo mi;
		mi.name="yield";
		mi.arguments.push_back(PropertyInfo(Variant::OBJECT,"object"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING,"signal"));
		mi.default_arguments.push_back(Variant::NIL);
		mi.default_arguments.push_back(Variant::STRING);
		p_functions->push_back(mi);
	}
	{
		MethodInfo mi;
		mi.name="assert";
		mi.arguments.push_back(PropertyInfo(Variant::BOOL,"condition"));
		p_functions->push_back(mi);
	}
}

void GVScriptLanguage::get_public_constants(List<Pair<String, Variant> > *p_constants) const {
	Pair<String,Variant> pi;
	pi.first="PI";
	pi.second=Math_PI;
	p_constants->push_back(pi);
}

void GVScriptLanguage::profiling_start() {
#ifdef DEBUG_ENABLED
	if (lock) {
		lock->lock();
	}

	SelfList<GDFunction> *elem=function_list.first();
	while(elem) {
		elem->self()->profile.call_count=0;
		elem->self()->profile.self_time=0;
		elem->self()->profile.total_time=0;
		elem->self()->profile.frame_call_count=0;
		elem->self()->profile.frame_self_time=0;
		elem->self()->profile.frame_total_time=0;
		elem->self()->profile.last_frame_call_count=0;
		elem->self()->profile.last_frame_self_time=0;
		elem->self()->profile.last_frame_total_time=0;
		elem=elem->next();
	}

	profiling=true;
	if (lock) {
		lock->unlock();
	}

#endif
}

void GVScriptLanguage::profiling_stop() {
#ifdef DEBUG_ENABLED
	if (lock) {
		lock->lock();
	}

	profiling=false;
	if (lock) {
		lock->unlock();
	}

#endif
}

int GVScriptLanguage::profiling_get_accumulated_data(ScriptLanguage::ProfilingInfo *p_info_arr, int p_info_max)
{
	int current=0;
#ifdef DEBUG_ENABLED
	if (lock) {
		lock->lock();
	}


	SelfList<GVFunction> *elem=function_list.first();
	while(elem) {
		if (current>=p_info_max)
			break;
		p_info_arr[current].call_count=elem->self()->profile.call_count;
		p_info_arr[current].self_time=elem->self()->profile.self_time;
		p_info_arr[current].total_time=elem->self()->profile.total_time;
		p_info_arr[current].signature=elem->self()->profile.signature;
		elem=elem->next();
		current++;
	}



	if (lock) {
		lock->unlock();
	}


#endif

	return current;
}

int GVScriptLanguage::profiling_get_frame_data(ScriptLanguage::ProfilingInfo *p_info_arr, int p_info_max)
{
	int current=0;

#ifdef DEBUG_ENABLED
	if (lock) {
		lock->lock();
	}


	SelfList<GVFunction> *elem=function_list.first();
	while(elem) {
		if (current>=p_info_max)
			break;
		if (elem->self()->profile.last_frame_call_count>0) {
			p_info_arr[current].call_count=elem->self()->profile.last_frame_call_count;
			p_info_arr[current].self_time=elem->self()->profile.last_frame_self_time;
			p_info_arr[current].total_time=elem->self()->profile.last_frame_total_time;
			p_info_arr[current].signature=elem->self()->profile.signature;
			//print_line(String(elem->self()->profile.signature)+": "+itos(elem->self()->profile.last_frame_call_count));
			current++;
		}
		elem=elem->next();

	}


	if (lock) {
		lock->unlock();
	}


#endif

	return current;
}

void GVScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const
{
	p_extensions->push_back("gv");
}

GVScriptLanguage::GVScriptLanguage()
{

	calls=0;
	ERR_FAIL_COND(singleton);
	singleton=this;
	strings._init = StaticCString::create("_init");
	strings._notification = StaticCString::create("_notification");
	strings._set= StaticCString::create("_set");
	strings._get= StaticCString::create("_get");
	strings._get_property_list= StaticCString::create("_get_property_list");
	//strings._script_source=StaticCString::create("script/source");
	_debug_parse_err_line=-1;
	_debug_parse_err_file="";

#ifdef NO_THREADS
	lock=NULL;
#else
	lock = Mutex::create();
#endif
	profiling=false;
	script_frame_time=0;

	_debug_call_stack_pos=0;
	int dmcs=GLOBAL_DEF("debug/script_max_call_stack",1024);
	if (ScriptDebugger::get_singleton()) {
		//debugging enabled!

		_debug_max_call_stack = dmcs;
		if (_debug_max_call_stack<1024)
			_debug_max_call_stack=1024;
		_call_stack = memnew_arr( CallLevel, _debug_max_call_stack+1 );

	} else {
		_debug_max_call_stack=0;
		_call_stack=NULL;
	}
}

GVScriptLanguage::~GVScriptLanguage()
{
	if (lock) {
		memdelete(lock);
		lock=NULL;
	}
	if (_call_stack)  {
		memdelete_arr(_call_stack);
	}
	singleton=NULL;
}

/*************** RESOURCE ***************/

RES ResourceFormatLoaderGVScript::load(const String &p_path, const String& p_original_path, Error *r_error) {

	if (r_error)
		*r_error=ERR_FILE_CANT_OPEN;

	GVScript *script = memnew( GVScript );

	Ref<GVScript> scriptres(script);

	if (p_path.ends_with(".gv")) {

		script->set_script_path(p_original_path); // script needs this.
		script->set_path(p_original_path);
		Error err = script->load_byte_code(p_path);


		if (err!=OK) {

			ERR_FAIL_COND_V(err!=OK, RES());
		}

	}
	if (r_error)
		*r_error=OK;

	return scriptres;
}
void ResourceFormatLoaderGVScript::get_recognized_extensions(List<String> *p_extensions) const {

	p_extensions->push_back("gv");
}

bool ResourceFormatLoaderGVScript::handles_type(const String& p_type) const {

	return (p_type=="Script" || p_type=="GVScript");
}

String ResourceFormatLoaderGVScript::get_resource_type(const String &p_path) const {

	String el = p_path.extension().to_lower();
	if (el=="gv")
		return "GVScript";
	return "";
}


Error ResourceFormatSaverGVScript::save(const String &p_path,const RES& p_resource,uint32_t p_flags) {

	Ref<GVScript> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(),ERR_INVALID_PARAMETER);

	Vector<uint8_t> source = sqscr->get_as_byte_code();

	Error err;
	FileAccess *file = FileAccess::open(p_path,FileAccess::WRITE,&err);


	if (err) {

		ERR_FAIL_COND_V(err,err);
	}

	file->store_string(source);
	if (file->get_error()!=OK && file->get_error()!=ERR_FILE_EOF) {
		memdelete(file);
		return ERR_CANT_CREATE;
	}
	file->close();
	memdelete(file);

	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		GVScriptLanguage::get_singleton()->reload_tool_script(p_resource,false);
	}

	return OK;
}

void ResourceFormatSaverGVScript::get_recognized_extensions(const RES& p_resource,List<String> *p_extensions) const {

	if (p_resource->cast_to<GVScript>()) {
		p_extensions->push_back("gv");
	}

}
bool ResourceFormatSaverGVScript::recognize(const RES& p_resource) const {

	return p_resource->cast_to<GVScript>()!=NULL;
}

