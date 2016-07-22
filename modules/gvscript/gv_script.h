#ifndef GV_SCRIPT_H
#define GV_SCRIPT_H

#include "core/script_language.h"
#include "core/self_list.h"
#include "core/os/thread.h"
#include "gv_function.h"

class GVNativeClass : public Reference {

	OBJ_TYPE(GVNativeClass,Reference);

	StringName name;
protected:

	bool _get(const StringName& p_name,Variant &r_ret) const;
	static void _bind_methods();

public:

	_FORCE_INLINE_ const StringName& get_name() const { return name; }
	Variant _new();
	Object *instance();
	GVNativeClass(const StringName& p_name);
};

class GVScript : public Script {

	OBJ_TYPE(GVScript,Script);
	bool tool;
	bool valid;

	struct MemberInfo {
		int index;
		Variant::Type type;
		Variant def_value;
		StringName setter;
		StringName getter;
	};

	Variant _static_ref; //used for static call
	Ref<GVNativeClass> native;
	Ref<GVScript> base;
	GVScript *_base; //fast pointer access
	GVScript *_owner; //for subclasses

	Set<StringName> members; //members are just indices to the instanced script.
	Map<StringName,MemberInfo> member_indices; //members are just indices to the instanced script.
	Map<StringName,Ref<GVScript> > subclasses;
	Map<StringName,Vector<StringName> > _signals;
	Map<StringName,Variant> constants;
	Map<StringName,GVFunction*> member_functions;
//#ifdef TOOLS_ENABLED NOTE::MARIANOGNU: UNCOMENT

	Map<StringName,Variant> member_default_values;

	List<PropertyInfo> members_cache;
	Map<StringName,Variant> member_default_values_cache;
	Ref<GVScript> base_cache;
	Set<ObjectID> inheriters_cache;
	bool source_changed_cache;
	void _update_exports_values(Map<StringName,Variant>& values, List<PropertyInfo> &propnames);

//#endif NOTE::MARIANOGNU: UNCOMENT
	Map<StringName,PropertyInfo> member_info;

	GVFunction *initializer; //direct pointer to _init , faster to locate

	int subclass_count;
	Set<Object*> instances;

	//exported members
	String source; //NOTE::MARIANOGNU: what's source for?
	String path;
	String name;
	GVInstance* _create_instance(const Variant** p_args,int p_argcount,Object *p_owner,bool p_isref,Variant::CallError &r_error);

	void _set_subclass_path(Ref<GVScript>& p_sc,const String& p_path);

#ifdef TOOLS_ENABLED
	Set<PlaceHolderScriptInstance*> placeholders;
	//void _update_placeholder(PlaceHolderScriptInstance *p_placeholder);
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder);
#endif

	bool _update_exports();

protected:
	bool _get(const StringName& p_name,Variant &r_ret) const;
	bool _set(const StringName& p_name, const Variant& p_value);
	void _get_property_list(List<PropertyInfo> *p_properties) const;

	Variant call(const StringName& p_method,const Variant** p_args,int p_argcount,Variant::CallError &r_error);
//	void call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount);

	static void _bind_methods();
public:

	bool is_valid() const { return valid; }

	const Map<StringName,Ref<GVScript> >& get_subclasses() const { return subclasses; }
	const Map<StringName,Variant >& get_constants() const { return constants; }
	const Set<StringName>& get_members() const { return members; }
	const Map<StringName,GVFunction*>& get_member_functions() const { return member_functions; }
	const Ref<GVNativeClass>& get_native() const { return native; }

	Error load_byte_code(const String& p_path);

	Vector<uint8_t> get_as_byte_code() const;

	GVScript();
};

class GVInstance : public ScriptInstance {
	Object *owner;
	Ref<GVScript> script;
#ifdef DEBUG_ENABLED
	Map<StringName,int> member_indices_cache; //used only for hot script reloading
#endif
	Vector<Variant> members;
	bool base_ref;

public:

	_FORCE_INLINE_ Object* get_owner() { return owner; }

	virtual bool set(const StringName& p_name, const Variant& p_value);
	virtual bool get(const StringName& p_name, Variant &r_ret) const;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const;
	virtual Variant::Type get_property_type(const StringName& p_name,bool *r_is_valid=NULL) const;


	virtual void get_method_list(List<MethodInfo> *p_list) const;
	virtual bool has_method(const StringName& p_method) const;
	virtual Variant call(const StringName& p_method,const Variant** p_args,int p_argcount,Variant::CallError &r_error);
	virtual void call_multilevel(const StringName& p_method,const Variant** p_args,int p_argcount);
	virtual void call_multilevel_reversed(const StringName& p_method,const Variant** p_args,int p_argcount);

	Variant debug_get_member_by_index(int p_idx) const { return members[p_idx]; }

	virtual void notification(int p_notification);

	virtual Ref<Script> get_script() const;

	virtual ScriptLanguage *get_language();

	void set_path(const String& p_path);

	void reload_members();

	GVInstance();
	~GVInstance();
};

class GVScriptLanguage : public ScriptLanguage {

	static GVScriptLanguage *singleton;

	Variant* _global_array;
	Vector<Variant> global_array;
	Map<StringName,int> globals;


	struct CallLevel {

		Variant *stack;
		GVFunction *function;
		GVInstance *instance;
		int *ip; //NOTE::MARIANOGNU: WTH IS THIS?
		int *line; //NOTE::MARIANOGNU: PROBABLY CHANGED FOR NODE_IDX

	};


	int _debug_parse_err_line;
	String _debug_parse_err_file;
	String _debug_error;
	int _debug_call_stack_pos;
	int _debug_max_call_stack;
	CallLevel *_call_stack;

	void _add_global(const StringName& p_name,const Variant& p_value);


	Mutex *lock;

friend class GVScript;

	SelfList<GVScript>::List script_list;
friend class GVFunction;

	SelfList<GVFunction>::List function_list;
	bool profiling;
	uint64_t script_frame_time;
public:


	int calls;

	bool debug_break(const String& p_error,bool p_allow_continue=true);
	bool debug_break_parse(const String& p_file, int p_line,const String& p_error);

	_FORCE_INLINE_ void enter_function(GVInstance *p_instance,GVFunction *p_function, Variant *p_stack, int *p_ip, int *p_line) {

		if (Thread::get_main_ID()!=Thread::get_caller_ID())
			return; //no support for other threads than main for now

		if (ScriptDebugger::get_singleton()->get_lines_left()>0 && ScriptDebugger::get_singleton()->get_depth()>=0)
			ScriptDebugger::get_singleton()->set_depth( ScriptDebugger::get_singleton()->get_depth() +1 );

		if (_debug_call_stack_pos >= _debug_max_call_stack) {
			//stack overflow
			_debug_error="Stack Overflow (Stack Size: "+itos(_debug_max_call_stack)+")";
			ScriptDebugger::get_singleton()->debug(this);
			return;
	}

		_call_stack[_debug_call_stack_pos].stack=p_stack;
		_call_stack[_debug_call_stack_pos].instance=p_instance;
		_call_stack[_debug_call_stack_pos].function=p_function;
		_call_stack[_debug_call_stack_pos].ip=p_ip;
		_call_stack[_debug_call_stack_pos].line=p_line;
		_debug_call_stack_pos++;
	}

	_FORCE_INLINE_ void exit_function() {

		if (Thread::get_main_ID()!=Thread::get_caller_ID())
			return; //no support for other threads than main for now

		if (ScriptDebugger::get_singleton()->get_lines_left()>0 && ScriptDebugger::get_singleton()->get_depth()>=0)
		ScriptDebugger::get_singleton()->set_depth( ScriptDebugger::get_singleton()->get_depth() -1 );

		if (_debug_call_stack_pos==0) {

			_debug_error="Stack Underflow (Engine Bug)";
			ScriptDebugger::get_singleton()->debug(this);
			return;
		}

		_debug_call_stack_pos--;
	}


	virtual Vector<StackInfo> debug_get_current_stack_info() {
		if (Thread::get_main_ID()!=Thread::get_caller_ID())
		return Vector<StackInfo>();

		Vector<StackInfo> csi;
		csi.resize(_debug_call_stack_pos);
		for(int i=0;i<_debug_call_stack_pos;i++) {
			csi[_debug_call_stack_pos-i-1].line=_call_stack[i].line?*_call_stack[i].line:0;
			csi[_debug_call_stack_pos-i-1].script=Ref<GVScript>(_call_stack[i].function->get_script());
		}
		return csi;
	}

	struct {

		StringName _init;
		StringName _notification;
		StringName _set;
		StringName _get;
		StringName _get_property_list;
		//StringName _script_source; NOTE::MARIANOGNU: missing strings for getting nodes and connections

	} strings;


	_FORCE_INLINE_ int get_global_array_size() const { return global_array.size(); }
	_FORCE_INLINE_ Variant* get_global_array() { return _global_array; }
	_FORCE_INLINE_ const Map<StringName,int>& get_global_map() { return globals; }

	_FORCE_INLINE_ static GVScriptLanguage *get_singleton() { return singleton; }

	virtual String get_name() const;

	/* LANGUAGE FUNCTIONS */
	virtual void init();
	virtual String get_type() const;
	virtual String get_extension() const;
	virtual Error execute_file(const String& p_path) ;
	virtual void finish();

	/* EDITOR FUNCTIONS */
	virtual void get_reserved_words(List<String> *p_words) const {} //Not needed
	virtual void get_comment_delimiters(List<String> *p_delimiters) const {} //Not needed
	virtual void get_string_delimiters(List<String> *p_delimiters) const {} //Not needed
	virtual String get_template(const String& p_class_name, const String& p_base_class_name) const;
	virtual bool validate(const String& p_script,int &r_line_error,int &r_col_error,String& r_test_error, const String& p_path="",List<String> *r_functions=NULL) const {return true;} //Not needed
	virtual Script *create_script() const {return memnew( GVScript );}
	virtual bool has_named_classes() const {return false;}
	virtual int find_function(const String& p_function,const String& p_code) const {return -1;} //TODO::MARIANOGNU: return GVNode id of the funciton. What's p_code for visual scripting?
	virtual String make_function(const String& p_class,const String& p_name,const StringArray& p_args) const; //TODO::MARIANOGNU: create a node
	virtual Error complete_code(const String& p_code, const String& p_base_path, Object*p_owner,List<String>* r_options,String& r_call_hint);//TODO
	virtual void auto_indent_code(String& p_code,int p_from_line,int p_to_line) const {} //Not needed
	virtual void add_global_constant(const StringName& p_variable,const Variant& p_value);


	/* DEBUGGER FUNCTIONS */

	virtual String debug_get_error() const {return _debug_error;}
	virtual int debug_get_stack_level_count() const {return _debug_call_stack_pos;}
	virtual int debug_get_stack_level_line(int p_level) const {return -1;} //TODO::MARIANOGNU: if i'm correct, should return position in current function...
	virtual String debug_get_stack_level_function(int p_level) const {return "";} //TODO
	virtual String debug_get_stack_level_source(int p_level) const {return "";} //TODO
	virtual void debug_get_stack_level_locals(int p_level,List<String> *p_locals, List<Variant> *p_values, int p_max_subitems=-1,int p_max_depth=-1) {} //TODO
	virtual void debug_get_stack_level_members(int p_level,List<String> *p_members, List<Variant> *p_values, int p_max_subitems=-1,int p_max_depth=-1) {} //TODO
	virtual void debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems=-1,int p_max_depth=-1) {} //TODO
	virtual String debug_parse_stack_level_expression(int p_level,const String& p_expression,int p_max_subitems=-1,int p_max_depth=-1) {return "";} //TODO

	virtual void reload_all_scripts();
	virtual void reload_tool_script(const Ref<Script>& p_script,bool p_soft_reload);

	virtual void frame();

	virtual void get_public_functions(List<MethodInfo> *p_functions) const;
	virtual void get_public_constants(List<Pair<String,Variant> > *p_constants) const;

	virtual void profiling_start();
	virtual void profiling_stop();

	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr,int p_info_max);
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr,int p_info_max);

	/* LOADER FUNCTIONS */

	virtual void get_recognized_extensions(List<String> *p_extensions) const;

	GVScriptLanguage();
	~GVScriptLanguage();
};

class ResourceFormatLoaderGVScript : public ResourceFormatLoader {
public:

	virtual RES load(const String &p_path,const String& p_original_path="",Error *r_error=NULL);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String& p_type) const;
	virtual String get_resource_type(const String &p_path) const;

};

class ResourceFormatSaverGVScript : public ResourceFormatSaver {
public:

	virtual Error save(const String &p_path,const RES& p_resource,uint32_t p_flags=0);
	virtual void get_recognized_extensions(const RES& p_resource,List<String> *p_extensions) const;
	virtual bool recognize(const RES& p_resource) const;

};
#endif // GV_SCRIPT_H
