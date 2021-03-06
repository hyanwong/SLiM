//
//  eidos_class_Dictionary.cpp
//  Eidos
//
//  Created by Ben Haller on 10/12/20.
//  Copyright (c) 2020-2021 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.

#include "eidos_class_Dictionary.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"
#include "eidos_value.h"
#include "json.hpp"

#include <utility>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <string>


//
// EidosDictionaryUnretained
//
#pragma mark -
#pragma mark EidosDictionaryUnretained
#pragma mark -

EidosDictionaryUnretained::EidosDictionaryUnretained(__attribute__((unused)) const EidosDictionaryUnretained &p_original) : EidosObject()
{
	// copy hash_symbols_ from p_original; I think this is called only when a Substitution is created from a Mutation
	if (p_original.hash_symbols_)
	{
		hash_symbols_ = new EidosDictionaryHashTable;
		
		*hash_symbols_ = *(p_original.hash_symbols_);
	}
}

std::string EidosDictionaryUnretained::Serialization(void) const
{
	// Loop through our key-value pairs and serialize them
	if (!hash_symbols_)
		return "";
	
	std::ostringstream ss;
	
	// We want to output our keys in the same order as allKeys, so we just use AllKeys()
	EidosValue_SP all_keys = AllKeys();
	EidosValue_String_vector *string_vec = dynamic_cast<EidosValue_String_vector *>(all_keys.get());
	
	if (!string_vec)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization): (internal error) allKeys did not return a string vector." << EidosTerminate(nullptr);
	
	const std::vector<std::string> *all_key_strings = string_vec->StringVector();
	
	for (const std::string &key : *all_key_strings)
	{
		// emit the key; quote the key string only if it contains characters that will make parsing difficult/ambiguous
		EidosStringQuoting key_quoting = EidosStringQuoting::kNoQuotes;
		
		if (key.find_first_of("\"\'\\\r\n\t =;") != std::string::npos)
			key_quoting = EidosStringQuoting::kDoubleQuotes;		// if we use quotes, always use double quotes, for ease of parsing
		
		ss << Eidos_string_escaped(key, key_quoting) << "=";
		
		// emit the value
		auto hash_iter = hash_symbols_->find(key);
		
		if (hash_iter == hash_symbols_->end())
		{
			// We assume that this is not an internal error, but is instead LogFile with a column that is NA;
			// it returns all of its column names for AllKeys() even if they have NA as a value, so we play along
			ss << "NA;";
		}
		else
		{
			ss << *(hash_iter->second) << ";";
		}
	}
	
	return ss.str();
}

nlohmann::json EidosDictionaryUnretained::JSONRepresentation(void) const
{
	nlohmann::json json_object;
	
	// We want to output our keys in the same order as allKeys, so we just use AllKeys()
	EidosValue_SP all_keys = AllKeys();
	EidosValue_String_vector *string_vec = dynamic_cast<EidosValue_String_vector *>(all_keys.get());
	
	if (!string_vec)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::Serialization): (internal error) allKeys did not return a string vector." << EidosTerminate(nullptr);
	
	const std::vector<std::string> *all_key_strings = string_vec->StringVector();
	
	for (const std::string &key : *all_key_strings)
	{
		// get the value
		auto hash_iter = hash_symbols_->find(key);
		
		if (hash_iter == hash_symbols_->end())
		{
			// We assume that this is not an internal error, but is instead LogFile with a column that is NA;
			// it returns all of its column names for AllKeys() even if they have NA as a value, so we play along
			json_object[key] = nullptr;
		}
		else
		{
			EidosValue *value = hash_iter->second.get();
			
			json_object[key] = value->JSONRepresentation();
		}
	}
	
	return json_object;
}

void EidosDictionaryUnretained::SetKeyValue(const std::string &key, EidosValue_SP value)
{
	EidosValueType value_type = value->Type();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also Eidos_ExecuteFunction_defineConstant() and Eidos_ExecuteFunction_defineGlobal(), which enforce the same rule
	if (value_type == EidosValueType::kValueObject)
	{
		const EidosClass *value_class = ((EidosValue_Object *)value.get())->Class();
		
		if (!value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::SetKeyValue): Dictionary can only accept object classes that are under retain/release memory management internally; class " << value_class->ClassName() << " is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map
		if (hash_symbols_)
			hash_symbols_->erase(key);
	}
	else
	{
		// Copy if necessary; see ExecuteMethod_Accelerated_setValue() for comments
		if ((value->UseCount() != 1) || value->Invisible())
			value = value->CopyValues();
		
		if (!hash_symbols_)
			hash_symbols_ = new EidosDictionaryHashTable;
		
		(*hash_symbols_)[key] = value;
	}
}

EidosValue_SP EidosDictionaryUnretained::AllKeys(void) const
{
	if (!hash_symbols_)
		return gStaticEidosValue_String_ZeroVec;
	
	int key_count = (int)hash_symbols_->size();
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve(key_count);
	
	for (auto const &kv_pair : *hash_symbols_)
		string_result->PushString(kv_pair.first);
	
	string_result->Sort(true);		// return keys in sorted order for convenience
	
	return EidosValue_SP(string_result);
}


//
// EidosDictionaryUnretained Eidos support
//
#pragma mark -
#pragma mark EidosDictionaryUnretained Eidos support
#pragma mark -

const EidosClass *EidosDictionaryUnretained::Class(void) const
{
	return gEidosDictionaryUnretained_Class;
}

void EidosDictionaryUnretained::Print(std::ostream &p_ostream) const
{
	p_ostream << "{" << Serialization() << "}";
}

EidosValue_SP EidosDictionaryUnretained::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gEidosID_allKeys:
			return AllKeys();
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue_SP EidosDictionaryUnretained::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gEidosID_addKeysAndValuesFrom:		return ExecuteMethod_addKeysAndValuesFrom(p_method_id, p_arguments, p_interpreter);
		case gEidosID_clearKeysAndValues:		return ExecuteMethod_clearKeysAndValues(p_method_id, p_arguments, p_interpreter);
		case gEidosID_getValue:					return ExecuteMethod_getValue(p_method_id, p_arguments, p_interpreter);
		//case gEidosID_setValue:				return ExecuteMethod_Accelerated_setValue(p_method_id, p_arguments, p_interpreter);
		case gEidosID_serialize:				return ExecuteMethod_serialize(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)addKeysAndValuesFrom(object$ source)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_addKeysAndValuesFrom(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *source_value = p_arguments[0].get();
	
	// Check that source is a subclass of EidosDictionaryUnretained.  We do this check here because we want to avoid making
	// EidosDictionaryUnretained visible in the public API; we want to pretend that there is just one class, Dictionary.
	// I'm not sure whether that's going to be right in the long term, but I want to keep my options open for now.
	EidosDictionaryUnretained *source = dynamic_cast<EidosDictionaryUnretained *>(source_value->ObjectElementAtIndex(0, nullptr));
	
	if (!source)
		EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_addKeysAndValuesFrom): addKeysAndValuesFrom() can only take values from a Dictionary or a subclass of Dictionary." << EidosTerminate(nullptr);
	
	// Loop through the key-value pairs of source and add them
	if (source->hash_symbols_)
	{
		for (auto const &kv_pair : *source->hash_symbols_)
		{
			const std::string &key = kv_pair.first;
			const EidosValue_SP &value = kv_pair.second;
			
			if (!hash_symbols_)
				hash_symbols_ = new EidosDictionaryHashTable;
			
			// BCH: Copy values just as EidosSymbolTable does, to prevent them from being modified underneath us etc.
			// If we have the only reference to the value, we don't need to copy it; otherwise we copy, since we don't want to hold
			// onto a reference that somebody else might modify under us (or that we might modify under them, with syntaxes like
			// x[2]=...; and x=x+1;). If the value is invisible then we copy it, since the symbol table never stores invisible values.
			if ((value->UseCount() != 1) || value->Invisible())
			{
				// Copy case
				(*hash_symbols_)[key] = value->CopyValues();
			}
			else
			{
				// No copy needed
				(*hash_symbols_)[key] = value;
			}
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)clearKeysAndValues(void)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_clearKeysAndValues(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (hash_symbols_)
		hash_symbols_->clear();
	
	return gStaticEidosValueVOID;
}

//	*********************	- (*)getValue(string $ key)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_getValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_String *key_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &key = key_value->StringRefAtIndex(0, nullptr);
	
	if (!hash_symbols_)
		return gStaticEidosValueNULL;
	
	auto found_iter = hash_symbols_->find(key);
	
	if (found_iter == hash_symbols_->end())
	{
		return gStaticEidosValueNULL;
	}
	else
	{
		return found_iter->second;
	}
}

//	*********************	- (void)setValue(string$ key, * value)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// Note that this does not call SetKeyValue() for speed, and to avoid excess copying
	
	EidosValue_String *key_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &key = key_value->StringRefAtIndex(0, nullptr);
	EidosValue_SP value = p_arguments[1];
	EidosValueType value_type = value->Type();
	
	// Object values can only be remembered if their class is under retain/release, so that we have control over the object lifetime
	// See also Eidos_ExecuteFunction_defineConstant() and Eidos_ExecuteFunction_defineGlobal(), which enforce the same rule
	if (value_type == EidosValueType::kValueObject)
	{
		const EidosClass *value_class = ((EidosValue_Object *)value.get())->Class();
		
		if (!value_class->UsesRetainRelease())
			EIDOS_TERMINATION << "ERROR (EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue): setValue() can only accept object classes that are under retain/release memory management internally; class " << value_class->ClassName() << " is not.  This restriction is necessary in order to guarantee that the kept object elements remain valid." << EidosTerminate(nullptr);
	}
	
	if (value_type == EidosValueType::kValueNULL)
	{
		// Setting a key to NULL removes it from the map
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			EidosDictionaryUnretained *element = (EidosDictionaryUnretained *)(p_elements[element_index]);
			
			if (element->hash_symbols_)
				element->hash_symbols_->erase(key);
		}
	}
	else
	{
		// BCH: Copy values just as EidosSymbolTable does, to prevent them from being modified underneath us etc.
		// Note that when setting a value across multiple object targets, however, they all receive the same copy.
		// That should be safe; there should be no way for that value to get modified after we have copied it.  1/28/2019
		// If we have the only reference to the value, we don't need to copy it; otherwise we copy, since we don't want to hold
		// onto a reference that somebody else might modify under us (or that we might modify under them, with syntaxes like
		// x[2]=...; and x=x+1;). If the value is invisible then we copy it, since the symbol table never stores invisible values.
		if ((value->UseCount() != 1) || value->Invisible())
			value = value->CopyValues();
		
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			EidosDictionaryUnretained *element = (EidosDictionaryUnretained *)(p_elements[element_index]);
			
			if (!element->hash_symbols_)
				element->hash_symbols_ = new EidosDictionaryHashTable;
			
			(*element->hash_symbols_)[key] = value;
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (string$)serialize(void)
//
EidosValue_SP EidosDictionaryUnretained::ExecuteMethod_serialize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(Serialization()));
}


//
//	EidosDictionaryUnretained_Class
//
#pragma mark -
#pragma mark EidosDictionaryUnretained_Class
#pragma mark -

EidosClass *gEidosDictionaryUnretained_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *EidosDictionaryUnretained_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_allKeys,				true,	kEidosValueMaskString)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *EidosDictionaryUnretained_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_addKeysAndValuesFrom, kEidosValueMaskVOID))->AddObject_S("source", nullptr));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_clearKeysAndValues, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_getValue, kEidosValueMaskAny))->AddString_S("key"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_serialize, kEidosValueMaskString | kEidosValueMaskSingleton)));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gEidosStr_setValue, kEidosValueMaskVOID))->AddString_S("key")->AddAny("value"))->DeclareAcceleratedImp(EidosDictionaryUnretained::ExecuteMethod_Accelerated_setValue));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}


//
// EidosDictionaryRetained
//
#pragma mark -
#pragma mark EidosDictionaryRetained
#pragma mark -

/*
EidosDictionaryRetained::EidosDictionaryRetained(void)
{
//	std::cerr << "EidosDictionaryRetained::EidosDictionaryRetained allocated " << this << " with refcount == 1" << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}

EidosDictionaryRetained::~EidosDictionaryRetained(void)
{
//	std::cerr << "EidosDictionaryRetained::~EidosDictionaryRetained deallocated " << this << std::endl;
//	Eidos_PrintStacktrace(stderr, 10);
}
*/

void EidosDictionaryRetained::SelfDelete(void)
{
	// called when our refcount reaches zero; can be overridden by subclasses to provide custom behavior
	// the default behavior assumes that this was allocated by new, and uses delete to free it
	delete this;
}

//	(object<Dictionary>$)Dictionary()
static EidosValue_SP Eidos_Instantiate_EidosDictionaryRetained(__attribute__((unused)) const std::vector<EidosValue_SP> &p_arguments, __attribute__((unused)) EidosInterpreter &p_interpreter)
{
	// Note that this function ignores matrix/array attributes, and always returns a vector, by design
	
	EidosValue_SP result_SP(nullptr);
	
	EidosDictionaryRetained *objectElement = new EidosDictionaryRetained();
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(objectElement, gEidosDictionaryRetained_Class));
	
	// objectElement is now retained by result_SP, so we can release it
	objectElement->Release();
	
	return result_SP;
}


//
// EidosDictionaryRetained Eidos support
//
#pragma mark -
#pragma mark EidosDictionaryRetained Eidos support
#pragma mark -

const EidosClass *EidosDictionaryRetained::Class(void) const
{
	return gEidosDictionaryRetained_Class;
}


//
//	EidosDictionaryRetained_Class
//
#pragma mark -
#pragma mark EidosDictionaryRetained_Class
#pragma mark -

EidosClass *gEidosDictionaryRetained_Class = nullptr;


const std::vector<EidosFunctionSignature_CSP> *EidosDictionaryRetained_Class::Functions(void) const
{
	static std::vector<EidosFunctionSignature_CSP> *functions = nullptr;
	
	if (!functions)
	{
		// Note there is no call to super, the way there is for methods and properties; functions are not inherited!
		functions = new std::vector<EidosFunctionSignature_CSP>;
		
		functions->emplace_back((EidosFunctionSignature *)(new EidosFunctionSignature(gEidosStr_Dictionary, Eidos_Instantiate_EidosDictionaryRetained, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosDictionaryRetained_Class)));
		
		std::sort(functions->begin(), functions->end(), CompareEidosCallSignatures);
	}
	
	return functions;
}

bool EidosDictionaryRetained_Class::UsesRetainRelease(void) const
{
	return true;
}










































