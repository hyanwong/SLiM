//
//  subpopulation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "subpopulation.h"
#include "slim_sim.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <string>
#include <map>


using std::string;
using std::endl;


// given the subpop size and sex ratio currently set for the child generation, make new genomes to fit
void Subpopulation::GenerateChildrenToFit(const bool p_parents_also)
{
#ifdef DEBUG
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
#endif
	
	// throw out whatever used to be there
	child_genomes_.clear();
	cached_child_genomes_value_.reset();
	
	if (p_parents_also)
	{
		parent_genomes_.clear();
		cached_parent_genomes_value_.reset();
	}
	
	// make new stuff
	if (sex_enabled_)
	{
		// Figure out the first male index from the sex ratio, and exit if we end up with all one sex
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(child_sex_ratio_ * child_subpop_size_));	// round in favor of males, arbitrarily
		
		child_first_male_index_ = child_subpop_size_ - total_males;
		
		if (child_first_male_index_ <= 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no females." << eidos_terminate();
		else if (child_first_male_index_ >= child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no males." << eidos_terminate();
		
		if (p_parents_also)
		{
			total_males = static_cast<slim_popsize_t>(lround(parent_sex_ratio_ * parent_subpop_size_));	// round in favor of males, arbitrarily
			
			parent_first_male_index_ = parent_subpop_size_ - total_males;
			
			if (parent_first_male_index_ <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no females." << eidos_terminate();
			else if (parent_first_male_index_ >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no males." << eidos_terminate();
		}
		
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:
			{
				// set up genomes of type GenomeType::kAutosome with a shared empty MutationRun for efficiency
				{
					child_genomes_.reserve(2 * child_subpop_size_);
					
					MutationRun *shared_empty_run = MutationRun::NewMutationRun();
					Genome aut_model = Genome(GenomeType::kAutosome, false, shared_empty_run);
					
					for (slim_popsize_t i = 0; i < child_subpop_size_; ++i)
					{
						child_genomes_.emplace_back(aut_model);
						child_genomes_.emplace_back(aut_model);
					}
				}
				
				if (p_parents_also)
				{
					parent_genomes_.reserve(2 * parent_subpop_size_);
					
					MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
					Genome aut_model_parental = Genome(GenomeType::kAutosome, false, shared_empty_run_parental);
					
					for (slim_popsize_t i = 0; i < parent_subpop_size_; ++i)
					{
						parent_genomes_.emplace_back(aut_model_parental);
						parent_genomes_.emplace_back(aut_model_parental);
					}
				}
				break;
			}
			case GenomeType::kXChromosome:
			case GenomeType::kYChromosome:
			{
				// if we are not modeling a given chromosome type, then instances of it are null – they will log and exit if used
				{
					child_genomes_.reserve(2 * child_subpop_size_);
					
					MutationRun *shared_empty_run = MutationRun::NewMutationRun();
					Genome x_model = Genome(GenomeType::kXChromosome, modeled_chromosome_type_ != GenomeType::kXChromosome, shared_empty_run);
					Genome y_model = Genome(GenomeType::kYChromosome, modeled_chromosome_type_ != GenomeType::kYChromosome, shared_empty_run);
					
					// females get two Xs
					for (slim_popsize_t i = 0; i < child_first_male_index_; ++i)
					{
						child_genomes_.emplace_back(x_model);
						child_genomes_.emplace_back(x_model);
					}
					
					// males get an X and a Y
					for (slim_popsize_t i = child_first_male_index_; i < child_subpop_size_; ++i)
					{
						child_genomes_.emplace_back(x_model);
						child_genomes_.emplace_back(y_model);
					}
				}
				
				if (p_parents_also)
				{
					parent_genomes_.reserve(2 * parent_subpop_size_);
					
					MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
					Genome x_model_parental = Genome(GenomeType::kXChromosome, modeled_chromosome_type_ != GenomeType::kXChromosome, shared_empty_run_parental);
					Genome y_model_parental = Genome(GenomeType::kYChromosome, modeled_chromosome_type_ != GenomeType::kYChromosome, shared_empty_run_parental);
					
					// females get two Xs
					for (slim_popsize_t i = 0; i < parent_first_male_index_; ++i)
					{
						parent_genomes_.emplace_back(x_model_parental);
						parent_genomes_.emplace_back(x_model_parental);
					}
					
					// males get an X and a Y
					for (slim_popsize_t i = parent_first_male_index_; i < parent_subpop_size_; ++i)
					{
						parent_genomes_.emplace_back(x_model_parental);
						parent_genomes_.emplace_back(y_model_parental);
					}
				}
				break;
			}
		}
	}
	else
	{
		// set up genomes of type GenomeType::kAutosome with a shared empty MutationRun for efficiency
		{
			child_genomes_.reserve(2 * child_subpop_size_);
			
			MutationRun *shared_empty_run = MutationRun::NewMutationRun();
			Genome aut_model = Genome(GenomeType::kAutosome, false, shared_empty_run);
			
			for (slim_popsize_t i = 0; i < child_subpop_size_; ++i)
			{
				child_genomes_.emplace_back(aut_model);
				child_genomes_.emplace_back(aut_model);
			}
		}
		
		if (p_parents_also)
		{
			parent_genomes_.reserve(2 * parent_subpop_size_);
			
			MutationRun *shared_empty_run_parental = MutationRun::NewMutationRun();
			Genome aut_model_parental = Genome(GenomeType::kAutosome, false, shared_empty_run_parental);
			
			for (slim_popsize_t i = 0; i < parent_subpop_size_; ++i)
			{
				parent_genomes_.emplace_back(aut_model_parental);
				parent_genomes_.emplace_back(aut_model_parental);
			}
		}
	}
	
#ifdef DEBUG
	Genome::LogGenomeCopyAndAssign(old_log);
#endif
	
	// This is also our cue to make sure that the individuals_ vectors are sufficiently large.  We just expand both to have as many individuals
	// as would be needed to hold either the child or the parental generation, and we only expand, never shrink.  Since SLiM does not actually
	// use these vectors for simulation dynamics, they do not need to be accurately sized, just sufficiently large.  By the way, if you're
	// wondering why we need separate individuals for the child and parental generations at all, it is because in modifyChild() callbacks and
	// similar situations, we need to be accessing tag values and genomes and such for the individuals in both generations at the same time.
	// They two generations therefore need to keep separate state, just as with Genome objects and other such state.
	slim_popsize_t max_subpop_size = std::max(child_subpop_size_, parent_subpop_size_);
	slim_popsize_t parent_individuals_size = (slim_popsize_t)parent_individuals_.size();
	slim_popsize_t child_individuals_size = (slim_popsize_t)child_individuals_.size();
	
#ifdef DEBUG
	old_log = Individual::LogIndividualCopyAndAssign(false);
#endif
	
	// BCH 7 November 2016: If we are going to resize a vector containing individuals, we have
	// to be quite careful, because the vector can resize, changing the addresses of all of the
	// Individual objects, which means that any cached pointers are invalid!  This is not an issue
	// with most SLiM objects since we don't store them in vectors, we allocate them.  We also
	// have to be careful with Genomes, which are also kept in vectors.
	if (parent_individuals_size < max_subpop_size)
	{
		// Clear any EidosValue cache of our individuals that we have made for the individuals property
		cached_parent_individuals_value_.reset();
		
		// Clear any cached EidosValue self-references inside the Individual objects themselves
		for (Individual &parent_ind : parent_individuals_)
			parent_ind.ClearCachedEidosValue();
		
		do
		{
			parent_individuals_.emplace_back(Individual(*this, parent_individuals_size));
			parent_individuals_size++;
		}
		while (parent_individuals_size < max_subpop_size);
	}
	
	if (child_individuals_size < max_subpop_size)
	{
		// Clear any EidosValue cache of our individuals that we have made for the individuals property
		cached_child_individuals_value_.reset();
		
		// Clear any cached EidosValue self-references inside the Individual objects themselves
		for (Individual &child_ind : child_individuals_)
			child_ind.ClearCachedEidosValue();
		
		do
		{
			child_individuals_.emplace_back(Individual(*this, child_individuals_size));
			child_individuals_size++;
		}
		while (child_individuals_size < max_subpop_size);
	}
	
#ifdef DEBUG
	Individual::LogIndividualCopyAndAssign(old_log);
#endif
}

Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size) : population_(p_population), subpopulation_id_(p_subpopulation_id), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size),
	self_symbol_(EidosGlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class)))
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random individuals, based initially on equal fitnesses
	cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
	cached_fitness_capacity_ = parent_subpop_size_;
	cached_fitness_size_ = parent_subpop_size_;
	
	double *fitness_buffer_ptr = cached_parental_fitness_;
	
	for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
		*(fitness_buffer_ptr++) = 1.0;
	
	lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
}

// SEX ONLY
Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff) :
population_(p_population), subpopulation_id_(p_subpopulation_id), sex_enabled_(true), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size), parent_sex_ratio_(p_sex_ratio), child_sex_ratio_(p_sex_ratio), modeled_chromosome_type_(p_modeled_chromosome_type), x_chromosome_dominance_coeff_(p_x_chromosome_dominance_coeff),
	self_symbol_(EidosGlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class)))
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random females, based initially on equal fitnesses
	cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
	cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
	cached_fitness_capacity_ = parent_subpop_size_;
	cached_fitness_size_ = parent_subpop_size_;
	
	double *fitness_buffer_ptr = cached_parental_fitness_;
	double *male_buffer_ptr = cached_male_fitness_;
	
	for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
	{
		*(fitness_buffer_ptr++) = 1.0;
		*(male_buffer_ptr++) = 0.0;				// this vector has 0 for all females, for mateChoice() callbacks
	}
	
	// Set up to draw random males, based initially on equal fitnesses
	slim_popsize_t num_males = parent_subpop_size_ - parent_first_male_index_;
	
	for (slim_popsize_t i = 0; i < num_males; i++)
	{
		*(fitness_buffer_ptr++) = 1.0;
		*(male_buffer_ptr++) = 1.0;
	}
	
	lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
	lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, cached_parental_fitness_ + parent_first_male_index_);
}


Subpopulation::~Subpopulation(void)
{
	//std::cout << "Subpopulation::~Subpopulation" << std::endl;
	
	if (lookup_parent_)
		gsl_ran_discrete_free(lookup_parent_);
	
	if (lookup_female_parent_)
		gsl_ran_discrete_free(lookup_female_parent_);
	
	if (lookup_male_parent_)
		gsl_ran_discrete_free(lookup_male_parent_);
	
	if (cached_parental_fitness_)
		free(cached_parental_fitness_);
	
	if (cached_male_fitness_)
		free(cached_male_fitness_);
}

void Subpopulation::UpdateFitness(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, std::vector<SLiMEidosBlock*> &p_global_fitness_callbacks)
{
	// This function calculates the population mean fitness as a side effect
	double totalFitness = 0.0;
	
	// Figure out our callback scenario: zero, one, or many?  See the comment below, above FitnessOfParentWithGenomeIndices_NoCallbacks(),
	// for more info on this complication.  Here we just figure out which version to call and set up for it.
	int fitness_callback_count = (int)p_fitness_callbacks.size();
	bool fitness_callbacks_exist = (fitness_callback_count > 0);
	bool single_fitness_callback = false;
	MutationType *single_callback_mut_type = nullptr;
	
	if (fitness_callback_count == 1)
	{
		slim_objectid_t mutation_type_id = p_fitness_callbacks[0]->mutation_type_id_;
		const std::map<slim_objectid_t,MutationType*> &mut_types = population_.sim_.MutationTypes();
		auto found_muttype_pair = mut_types.find(mutation_type_id);
		
		if (found_muttype_pair != mut_types.end())
		{
			if (mut_types.size() > 1)
			{
				// We have a single callback that applies to a known mutation type among more than one defined type; we can optimize that
				single_fitness_callback = true;
				single_callback_mut_type = found_muttype_pair->second;
			}
			// else there is only one mutation type, so the callback applies to all mutations in the simulation
		}
		else
		{
			// The only callback refers to a mutation type that doesn't exist, so we effectively have no callbacks; we probably never hit this
			fitness_callback_count = 0;
			fitness_callbacks_exist = false;
		}
	}
	
	// Figure out global callbacks; these are callbacks with NULL supplied for the mut-type id, which means that they are called
	// exactly once per individual, for every individual regardless of genetics, to provide an entry point for alternate fitness definitions
	int global_fitness_callback_count = (int)p_global_fitness_callbacks.size();
	bool global_fitness_callbacks_exist = (global_fitness_callback_count > 0);
	
	// We cache the calculated fitness values, for use in PopulationView and mateChoice() callbacks and such
	if (cached_fitness_capacity_ < parent_subpop_size_)
	{
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		if (sex_enabled_)
			cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
		cached_fitness_capacity_ = parent_subpop_size_;
	}
	
	cached_fitness_size_ = 0;	// while we're refilling, the fitness cache is invalid
	
	// We optimize the pure neutral case, as long as no fitness callbacks are defined; fitness values are then simply 1.0, for everybody.
	bool pure_neutral = (!fitness_callbacks_exist && !global_fitness_callbacks_exist && population_.sim_.pure_neutral_);
	
	// calculate fitnesses in parent population and create new lookup table
	if (sex_enabled_)
	{
		// SEX ONLY
		double totalMaleFitness = 0.0, totalFemaleFitness = 0.0;
		
		gsl_ran_discrete_free(lookup_female_parent_);
		lookup_female_parent_ = nullptr;
		
		gsl_ran_discrete_free(lookup_male_parent_);
		lookup_male_parent_ = nullptr;
		
		// Set up to draw random females
		for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
		{
			double fitness;
			
			if (pure_neutral)
				fitness = 1.0;
			else
			{
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(i);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(i, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(i, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
			}
			
			cached_parental_fitness_[i] = fitness;
			cached_male_fitness_[i] = 0;				// this vector has 0 for all females, for mateChoice() callbacks
			
			totalFemaleFitness += fitness;
		}
		
		totalFitness += totalFemaleFitness;
		if (totalFemaleFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of females is <= 0.0." << eidos_terminate(nullptr);
		
		lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
		
		// Set up to draw random males
		slim_popsize_t num_males = parent_subpop_size_ - parent_first_male_index_;
		
		for (slim_popsize_t i = 0; i < num_males; i++)
		{
			slim_popsize_t individual_index = (i + parent_first_male_index_);
			double fitness;
			
			if (pure_neutral)
				fitness = 1.0;
			else
			{
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(individual_index, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(individual_index, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, individual_index);
			}
			
			cached_parental_fitness_[individual_index] = fitness;
			cached_male_fitness_[individual_index] = fitness;
			
			totalMaleFitness += fitness;
		}
		
		totalFitness += totalMaleFitness;
		if (totalMaleFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of males is <= 0.0." << eidos_terminate(nullptr);
		
		lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, cached_parental_fitness_ + parent_first_male_index_);
	}
	else
	{
		double *fitness_buffer_ptr = cached_parental_fitness_;
		
		gsl_ran_discrete_free(lookup_parent_);
		lookup_parent_ = nullptr;
		
		for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
		{
			double fitness;
			
			if (pure_neutral)
				fitness = 1.0;
			else
			{
				if (!fitness_callbacks_exist)
					fitness = FitnessOfParentWithGenomeIndices_NoCallbacks(i);
				else if (single_fitness_callback)
					fitness = FitnessOfParentWithGenomeIndices_SingleCallback(i, p_fitness_callbacks, single_callback_mut_type);
				else
					fitness = FitnessOfParentWithGenomeIndices_Callbacks(i, p_fitness_callbacks);
				
				// multiply in the effects of any global fitness callbacks (muttype==NULL)
				if (global_fitness_callbacks_exist && (fitness > 0.0))
					fitness *= ApplyGlobalFitnessCallbacks(p_global_fitness_callbacks, i);
			}
			
			*(fitness_buffer_ptr++) = fitness;
			
			totalFitness += fitness;
		}
		
		if (totalFitness <= 0.0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of all individuals is <= 0.0." << eidos_terminate(nullptr);
		
		lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
	}
	
	cached_fitness_size_ = parent_subpop_size_;
	
#ifdef SLIMGUI
	parental_total_fitness_ = totalFitness;
#endif
}

double Subpopulation::ApplyFitnessCallbacks(Mutation *p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, Individual *p_individual, Genome *p_genome1, Genome *p_genome2)
{
	slim_objectid_t mutation_type_id = p_mutation->mutation_type_ptr_->mutation_type_id_;
	SLiMSim &sim = population_.sim_;
	
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			slim_objectid_t callback_mutation_type_id = fitness_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
				// The callback is active and matches the mutation type id of the mutation, so we need to execute it
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
					EidosValue_SP result_SP = compound_statement_node->cached_value_;
					EidosValue *result = result_SP.get();
					
					if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << eidos_terminate(fitness_callback->identifier_token_);
					
					p_computed_fitness = result->FloatAtIndex(0, nullptr);
					
					// the cached value is owned by the tree, so we do not dispose of it
					// there is also no script output to handle
				}
				else
				{
					// local variables for the callback parameters that we might need to allocate here, and thus need to free below
					EidosValue_Object_singleton local_mut(p_mutation, gSLiM_Mutation_Class);
					EidosValue_Float_singleton local_relFitness(p_computed_fitness);
					
					// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
					{
						EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &population_.sim_.SymbolTable());
						EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
						EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
						EidosInterpreter interpreter(fitness_callback->compound_statement_node_, client_symbols, *function_map, &sim);
						
						if (fitness_callback->contains_self_)
							callback_symbols.InitializeConstantSymbolEntry(fitness_callback->SelfSymbolTableEntry());		// define "self"
						
						// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
						// We can use that method because we know the lifetime of the symbol table is shorter than that of
						// the value objects, and we know that the values we are setting here will not change (the objects
						// referred to by the values may change, but the values themselves will not change).
						if (fitness_callback->contains_mut_)
						{
							local_mut.stack_allocated();			// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_mut, EidosValue_SP(&local_mut));
						}
						if (fitness_callback->contains_relFitness_)
						{
							local_relFitness.stack_allocated();		// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_relFitness, EidosValue_SP(&local_relFitness));
						}
						if (fitness_callback->contains_individual_)
							callback_symbols.InitializeConstantSymbolEntry(gID_individual, p_individual->CachedEidosValue());
						if (fitness_callback->contains_genome1_)
							callback_symbols.InitializeConstantSymbolEntry(gID_genome1, p_genome1->CachedEidosValue());
						if (fitness_callback->contains_genome2_)
							callback_symbols.InitializeConstantSymbolEntry(gID_genome2, p_genome2->CachedEidosValue());
						if (fitness_callback->contains_subpop_)
							callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
						
						// p_homozygous == -1 means the mutation is opposed by a NULL chromosome; otherwise, 0 means heterozyg., 1 means homozyg.
						// that gets translated into Eidos values of NULL, F, and T, respectively
						if (fitness_callback->contains_homozygous_)
						{
							if (p_homozygous == -1)
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, gStaticEidosValueNULL);
							else
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, (p_homozygous != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
						}
						
						try
						{
							// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
							EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(fitness_callback->script_);
							EidosValue *result = result_SP.get();
							
							if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
								EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << eidos_terminate(fitness_callback->identifier_token_);
							
							p_computed_fitness = result->FloatAtIndex(0, nullptr);
							
							// Output generated by the interpreter goes to our output stream
							SLIM_OUTSTREAM << interpreter.ExecutionOutput();
						}
						catch (...)
						{
							// Emit final output even on a throw, so that stop() messages and such get printed
							SLIM_OUTSTREAM << interpreter.ExecutionOutput();
							
							throw;
						}
						
					}
				}
			}
		}
	}
	
	return p_computed_fitness;
}

// This calculates the effects of global fitness callbacks, i.e. those with muttype==NULL and which therefore do not reference any mutation
double Subpopulation::ApplyGlobalFitnessCallbacks(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, slim_popsize_t p_individual_index)
{
	double computed_fitness = 1.0;
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	SLiMSim &sim = population_.sim_;
	
	for (SLiMEidosBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			// The callback is active, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			const EidosASTNode *compound_statement_node = fitness_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << eidos_terminate(fitness_callback->identifier_token_);
				
				computed_fitness *= result->FloatAtIndex(0, nullptr);
				
				// the cached value is owned by the tree, so we do not dispose of it
				// there is also no script output to handle
			}
			else
			{
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &population_.sim_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
					EidosInterpreter interpreter(fitness_callback->compound_statement_node_, client_symbols, *function_map, &sim);
					
					if (fitness_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(fitness_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (fitness_callback->contains_mut_)
						callback_symbols.InitializeConstantSymbolEntry(gID_mut, gStaticEidosValueNULL);
					if (fitness_callback->contains_relFitness_)
						callback_symbols.InitializeConstantSymbolEntry(gID_relFitness, gStaticEidosValue_Float1);
					if (fitness_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, individual->CachedEidosValue());
					if (fitness_callback->contains_genome1_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome1, genome1->CachedEidosValue());
					if (fitness_callback->contains_genome2_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome2, genome2->CachedEidosValue());
					if (fitness_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					if (fitness_callback->contains_homozygous_)
						callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, gStaticEidosValueNULL);
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(fitness_callback->script_);
						EidosValue *result = result_SP.get();
						
						if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyGlobalFitnessCallbacks): fitness() callbacks must provide a float singleton return value." << eidos_terminate(fitness_callback->identifier_token_);
						
						computed_fitness *= result->FloatAtIndex(0, nullptr);
						
						// Output generated by the interpreter goes to our output stream
						SLIM_OUTSTREAM << interpreter.ExecutionOutput();
					}
					catch (...)
					{
						// Emit final output even on a throw, so that stop() messages and such get printed
						SLIM_OUTSTREAM << interpreter.ExecutionOutput();
						
						throw;
					}
					
				}
			}
			
			// If any callback puts us at or below zero, we can short-circuit the rest
			if (computed_fitness <= 0.0)
				return 0.0;
		}
	}
	
	return computed_fitness;
}

// FitnessOfParentWithGenomeIndices has three versions, for no callbacks, a single callback, and multiple callbacks.  This is for two reasons.  First,
// it allows the case without fitness() callbacks to run at full speed.  Second, the non-callback case short-circuits when the selection coefficient
// is exactly 0.0f, as an optimization; but that optimization would be invalid in the callback case, since callbacks can change the relative fitness
// of ostensibly neutral mutations.  For reasons of maintainability, the three versions should be kept in synch as closely as possible.
//
// When there is just a single callback, it usually refers to a mutation type that is relatively uncommon.  The model might have neutral mutations in most
// cases, plus a rare (or unique) mutation type that is subject to more complex selection, for example.  We can optimize that very common case substantially
// by making the callout to ApplyFitnessCallbacks() only for mutations of the mutation type that the callback modifies.  This pays off mostly when there
// are many common mutations with no callback, plus one rare mutation type that has a callback.  A model of neutral drift across a long chromosome with a
// high mutation rate, with an introduced beneficial mutation with a selection coefficient extremely close to 0, for example, would hit this case hard and
// see a speedup of as much as 25%, so the additional complexity seems worth it (since that's quite a realistic and common case).

// This version of FitnessOfParentWithGenomeIndices assumes no callbacks exist.  It tests for neutral mutations and skips processing them.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		Mutation *const *genome_iter = genome->begin_pointer_const();
		Mutation *const *genome_max = genome->end_pointer_const();
		
		if (genome->Type() == GenomeType::kXChromosome)
		{
			// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
			while (genome_iter != genome_max)
			{
				const Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome_iter++;
			}
		}
		else
		{
			// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
			while (genome_iter != genome_max)
			{
				const Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome_iter++;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		Mutation *const *genome1_iter = genome1->begin_pointer_const();
		Mutation *const *genome2_iter = genome2->begin_pointer_const();
		
		Mutation *const *genome1_max = genome1->end_pointer_const();
		Mutation *const *genome2_max = genome2->end_pointer_const();
		
		// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
		if (genome1_iter != genome1_max && genome2_iter != genome2_max)
		{
			Mutation *genome1_mutation = *genome1_iter, *genome2_mutation = *genome2_iter;
			slim_position_t genome1_iter_position = genome1_mutation->position_, genome2_iter_position = genome2_mutation->position_;
			
			do
			{
				if (genome1_iter_position < genome2_iter_position)
				{
					// Process a mutation in genome1 since it is leading
					slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
					
					genome1_iter++;
					
					if (genome1_iter == genome1_max)
						break;
					else {
						genome1_mutation = *genome1_iter;
						genome1_iter_position = genome1_mutation->position_;
					}
				}
				else if (genome1_iter_position > genome2_iter_position)
				{
					// Process a mutation in genome2 since it is leading
					slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
					
					genome2_iter++;
					
					if (genome2_iter == genome2_max)
						break;
					else {
						genome2_mutation = *genome2_iter;
						genome2_iter_position = genome2_mutation->position_;
					}
				}
				else
				{
					// Look for homozygosity: genome1_iter_position == genome2_iter_position
					slim_position_t position = genome1_iter_position;
					Mutation *const *genome1_start = genome1_iter;
					
					// advance through genome1 as long as we remain at the same position, handling one mutation at a time
					do
					{
						slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
						
						if (selection_coeff != 0.0f)
						{
							Mutation *const *genome2_matchscan = genome2_iter; 
							bool homozygous = false;
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									w *= (1.0 + selection_coeff);
									homozygous = true;
									
									if (w <= 0.0)
										return 0.0;
									
									break;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							if (!homozygous)
							{
								w *= (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
								
								if (w <= 0.0)
									return 0.0;
							}
						}
						
						genome1_iter++;
						
						if (genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = genome1_mutation->position_;
						}
					} while (genome1_iter_position == position);
					
					// advance through genome2 as long as we remain at the same position, handling one mutation at a time
					do
					{
						slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
						
						if (selection_coeff != 0.0f)
						{
							Mutation *const *genome1_matchscan = genome1_start; 
							bool homozygous = false;
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									homozygous = true;
									break;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							if (!homozygous)
							{
								w *= (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
								
								if (w <= 0.0)
									return 0.0;
							}
						}
						
						genome2_iter++;
						
						if (genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = genome2_mutation->position_;
						}
					} while (genome2_iter_position == position);
					
					// break out if either genome has reached its end
					if (genome1_iter == genome1_max || genome2_iter == genome2_max)
						break;
				}
			} while (true);
		}
		
		// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
		assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
		
		// if genome1 is unfinished, finish it
		while (genome1_iter != genome1_max)
		{
			const Mutation *genome1_mutation = *genome1_iter;
			slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
			
			if (selection_coeff != 0.0f)
			{
				w *= (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
				
				if (w <= 0.0)
					return 0.0;
			}
			
			genome1_iter++;
		}
		
		// if genome2 is unfinished, finish it
		while (genome2_iter != genome2_max)
		{
			const Mutation *genome2_mutation = *genome2_iter;
			slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
			
			if (selection_coeff != 0.0f)
			{
				w *= (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
				
				if (w <= 0.0)
					return 0.0;
			}
			
			genome2_iter++;
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes multiple callbacks exist.  It doesn't optimize neutral mutations since they might be modified by callbacks.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		Mutation *const *genome_iter = genome->begin_pointer_const();
		Mutation *const *genome_max = genome->end_pointer_const();
		
		if (genome->Type() == GenomeType::kXChromosome)
		{
			// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
			while (genome_iter != genome_max)
			{
				Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				double rel_fitness = (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome_iter++;
			}
		}
		else
		{
			// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
			while (genome_iter != genome_max)
			{
				Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				double rel_fitness = (1.0 + selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome_iter++;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		Mutation *const *genome1_iter = genome1->begin_pointer_const();
		Mutation *const *genome2_iter = genome2->begin_pointer_const();
		
		Mutation *const *genome1_max = genome1->end_pointer_const();
		Mutation *const *genome2_max = genome2->end_pointer_const();
		
		// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
		if (genome1_iter != genome1_max && genome2_iter != genome2_max)
		{
			Mutation *genome1_mutation = *genome1_iter, *genome2_mutation = *genome2_iter;
			slim_position_t genome1_iter_position = genome1_mutation->position_, genome2_iter_position = genome2_mutation->position_;
			
			do
			{
				if (genome1_iter_position < genome2_iter_position)
				{
					// Process a mutation in genome1 since it is leading
					slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
					double rel_fitness = (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome1_iter++;
					
					if (genome1_iter == genome1_max)
						break;
					else {
						genome1_mutation = *genome1_iter;
						genome1_iter_position = genome1_mutation->position_;
					}
				}
				else if (genome1_iter_position > genome2_iter_position)
				{
					// Process a mutation in genome2 since it is leading
					slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
					double rel_fitness = (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome2_iter++;
					
					if (genome2_iter == genome2_max)
						break;
					else {
						genome2_mutation = *genome2_iter;
						genome2_iter_position = genome2_mutation->position_;
					}
				}
				else
				{
					// Look for homozygosity: genome1_iter_position == genome2_iter_position
					slim_position_t position = genome1_iter_position;
					Mutation *const *genome1_start = genome1_iter;
					
					// advance through genome1 as long as we remain at the same position, handling one mutation at a time
					do
					{
						Mutation *const *genome2_matchscan = genome2_iter; 
						bool homozygous = false;
						
						// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
						while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
						{
							if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
							{
								// a match was found, so we multiply our fitness by the full selection coefficient
								double rel_fitness = (1.0 + genome1_mutation->selection_coeff_);
								
								w *= ApplyFitnessCallbacks(genome1_mutation, true, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
								homozygous = true;
								
								if (w <= 0.0)
									return 0.0;
								
								break;
							}
							
							genome2_matchscan++;
						}
						
						// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
						if (!homozygous)
						{
							double rel_fitness = (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * genome1_mutation->selection_coeff_);
							
							w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
							
							if (w <= 0.0)
								return 0.0;
						}
						
						genome1_iter++;
						
						if (genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = genome1_mutation->position_;
						}
					} while (genome1_iter_position == position);
					
					// advance through genome2 as long as we remain at the same position, handling one mutation at a time
					do
					{
						Mutation *const *genome1_matchscan = genome1_start; 
						bool homozygous = false;
						
						// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
						while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
						{
							if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
							{
								// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
								homozygous = true;
								break;
							}
							
							genome1_matchscan++;
						}
						
						// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
						if (!homozygous)
						{
							double rel_fitness = (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * genome2_mutation->selection_coeff_);
							
							w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
							
							if (w <= 0.0)
								return 0.0;
						}
						
						genome2_iter++;
						
						if (genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = genome2_mutation->position_;
						}
					} while (genome2_iter_position == position);
					
					// break out if either genome has reached its end
					if (genome1_iter == genome1_max || genome2_iter == genome2_max)
						break;
				}
			} while (true);
		}
		
		// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
		assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
		
		// if genome1 is unfinished, finish it
		while (genome1_iter != genome1_max)
		{
			Mutation *genome1_mutation = *genome1_iter;
			slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
			double rel_fitness = (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
			
			w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
			
			if (w <= 0.0)
				return 0.0;
			
			genome1_iter++;
		}
		
		// if genome2 is unfinished, finish it
		while (genome2_iter != genome2_max)
		{
			Mutation *genome2_mutation = *genome2_iter;
			slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
			double rel_fitness = (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
			
			w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
			
			if (w <= 0.0)
				return 0.0;
			
			genome2_iter++;
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes a single callback exists, modifying the given mutation type.  It is a hybrid of the previous two versions.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, MutationType *p_single_callback_mut_type)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	Individual *individual = &(parent_individuals_[p_individual_index]);
	Genome *genome1 = &(parent_genomes_[p_individual_index * 2]);
	Genome *genome2 = &(parent_genomes_[p_individual_index * 2 + 1]);
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		Mutation *const *genome_iter = genome->begin_pointer_const();
		Mutation *const *genome_max = genome->end_pointer_const();
		
		if (genome->Type() == GenomeType::kXChromosome)
		{
			// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
			while (genome_iter != genome_max)
			{
				Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				
				if (genome_mutation->mutation_type_ptr_ == p_single_callback_mut_type)
				{
					double rel_fitness = (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
				}
				
				genome_iter++;
			}
		}
		else
		{
			// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
			while (genome_iter != genome_max)
			{
				Mutation *genome_mutation = *genome_iter;
				slim_selcoeff_t selection_coeff = genome_mutation->selection_coeff_;
				
				if (genome_mutation->mutation_type_ptr_ == p_single_callback_mut_type)
				{
					double rel_fitness = (1.0 + selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
				}
				
				genome_iter++;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		Mutation *const *genome1_iter = genome1->begin_pointer_const();
		Mutation *const *genome2_iter = genome2->begin_pointer_const();
		
		Mutation *const *genome1_max = genome1->end_pointer_const();
		Mutation *const *genome2_max = genome2->end_pointer_const();
		
		// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
		if (genome1_iter != genome1_max && genome2_iter != genome2_max)
		{
			Mutation *genome1_mutation = *genome1_iter, *genome2_mutation = *genome2_iter;
			slim_position_t genome1_iter_position = genome1_mutation->position_, genome2_iter_position = genome2_mutation->position_;
			
			do
			{
				if (genome1_iter_position < genome2_iter_position)
				{
					// Process a mutation in genome1 since it is leading
					slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
					MutationType *genome1_muttype = genome1_mutation->mutation_type_ptr_;
					
					if (genome1_muttype == p_single_callback_mut_type)
					{
						double rel_fitness = (1.0 + genome1_muttype->dominance_coeff_ * selection_coeff);
						
						w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
					}
					else
					{
						if (selection_coeff != 0.0f)
						{
							w *= (1.0 + genome1_muttype->dominance_coeff_ * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
					}
					
					genome1_iter++;
					
					if (genome1_iter == genome1_max)
						break;
					else {
						genome1_mutation = *genome1_iter;
						genome1_iter_position = genome1_mutation->position_;
					}
				}
				else if (genome1_iter_position > genome2_iter_position)
				{
					// Process a mutation in genome2 since it is leading
					slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
					MutationType *genome2_muttype = genome2_mutation->mutation_type_ptr_;
					
					if (genome2_muttype == p_single_callback_mut_type)
					{
						double rel_fitness = (1.0 + genome2_muttype->dominance_coeff_ * selection_coeff);
						
						w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
					}
					else
					{
						if (selection_coeff != 0.0f)
						{
							w *= (1.0 + genome2_muttype->dominance_coeff_ * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
					}
					
					genome2_iter++;
					
					if (genome2_iter == genome2_max)
						break;
					else {
						genome2_mutation = *genome2_iter;
						genome2_iter_position = genome2_mutation->position_;
					}
				}
				else
				{
					// Look for homozygosity: genome1_iter_position == genome2_iter_position
					slim_position_t position = genome1_iter_position;
					Mutation *const *genome1_start = genome1_iter;
					
					// advance through genome1 as long as we remain at the same position, handling one mutation at a time
					do
					{
						MutationType *genome1_muttype = genome1_mutation->mutation_type_ptr_;
						
						if (genome1_muttype == p_single_callback_mut_type)
						{
							Mutation *const *genome2_matchscan = genome2_iter; 
							bool homozygous = false;
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									double rel_fitness = (1.0 + genome1_mutation->selection_coeff_);
									
									w *= ApplyFitnessCallbacks(genome1_mutation, true, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
									homozygous = true;
									
									if (w <= 0.0)
										return 0.0;
									
									break;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							if (!homozygous)
							{
								double rel_fitness = (1.0 + genome1_muttype->dominance_coeff_ * genome1_mutation->selection_coeff_);
								
								w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
								
								if (w <= 0.0)
									return 0.0;
							}
						}
						else
						{
							slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
							
							if (selection_coeff != 0.0f)
							{
								Mutation *const *genome2_matchscan = genome2_iter; 
								bool homozygous = false;
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
								{
									if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= (1.0 + selection_coeff);
										homozygous = true;
										
										if (w <= 0.0)
											return 0.0;
										
										break;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								if (!homozygous)
								{
									w *= (1.0 + genome1_muttype->dominance_coeff_ * selection_coeff);
									
									if (w <= 0.0)
										return 0.0;
								}
							}
						}
						
						genome1_iter++;
						
						if (genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = genome1_mutation->position_;
						}
					} while (genome1_iter_position == position);
					
					// advance through genome2 as long as we remain at the same position, handling one mutation at a time
					do
					{
						MutationType *genome2_muttype = genome2_mutation->mutation_type_ptr_;
						
						if (genome2_muttype == p_single_callback_mut_type)
						{
							Mutation *const *genome1_matchscan = genome1_start; 
							bool homozygous = false;
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									homozygous = true;
									break;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							if (!homozygous)
							{
								double rel_fitness = (1.0 + genome2_muttype->dominance_coeff_ * genome2_mutation->selection_coeff_);
								
								w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
								
								if (w <= 0.0)
									return 0.0;
							}
						}
						else
						{
							slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
							
							if (selection_coeff != 0.0f)
							{
								Mutation *const *genome1_matchscan = genome1_start; 
								bool homozygous = false;
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
								{
									if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
									{
										// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
										homozygous = true;
										break;
									}
									
									genome1_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								if (!homozygous)
								{
									w *= (1.0 + genome2_muttype->dominance_coeff_ * selection_coeff);
									
									if (w <= 0.0)
										return 0.0;
								}
							}
						}
						
						genome2_iter++;
						
						if (genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = genome2_mutation->position_;
						}
					} while (genome2_iter_position == position);
					
					// break out if either genome has reached its end
					if (genome1_iter == genome1_max || genome2_iter == genome2_max)
						break;
				}
			} while (true);
		}
		
		// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
		assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
		
		// if genome1 is unfinished, finish it
		while (genome1_iter != genome1_max)
		{
			Mutation *genome1_mutation = *genome1_iter;
			slim_selcoeff_t selection_coeff = genome1_mutation->selection_coeff_;
			MutationType *genome1_muttype = genome1_mutation->mutation_type_ptr_;
			
			if (genome1_muttype == p_single_callback_mut_type)
			{
				double rel_fitness = (1.0 + genome1_muttype->dominance_coeff_ * selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
			}
			else
			{
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + genome1_muttype->dominance_coeff_ * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
			}
			
			genome1_iter++;
		}
		
		// if genome2 is unfinished, finish it
		while (genome2_iter != genome2_max)
		{
			Mutation *genome2_mutation = *genome2_iter;
			slim_selcoeff_t selection_coeff = genome2_mutation->selection_coeff_;
			MutationType *genome2_muttype = genome2_mutation->mutation_type_ptr_;
			
			if (genome2_muttype == p_single_callback_mut_type)
			{
				double rel_fitness = (1.0 + genome2_muttype->dominance_coeff_ * selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, individual, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
			}
			else
			{
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + genome2_muttype->dominance_coeff_ * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
			}
			
			genome2_iter++;
		}
		
		return w;
	}
}

void Subpopulation::SwapChildAndParentGenomes(void)
{
	bool will_need_new_children = false;
	
	// If there are any differences between the parent and child genome setups (due to change in subpop size, sex ratio, etc.), we will need to create new child genomes after swapping
	// This is because the parental genomes, which are based on the old parental values, will get swapped in to the children, but they will be out of date.
	if (parent_subpop_size_ != child_subpop_size_ || parent_sex_ratio_ != child_sex_ratio_ || parent_first_male_index_ != child_first_male_index_)
		will_need_new_children = true;
	
	// Execute the genome swap
	child_genomes_.swap(parent_genomes_);
	cached_child_genomes_value_.swap(cached_parent_genomes_value_);
	
	// Execute a swap of individuals as well; since individuals carry so little baggage, this is mostly important just for moving tag values
	child_individuals_.swap(parent_individuals_);
	cached_child_individuals_value_.swap(cached_parent_individuals_value_);
	
	// Clear out any dictionary values and color values stored in what are now the child individuals
	for (Individual &child : child_individuals_)
	{
		child.RemoveAllKeys();
		child.ClearColor();
	}
	
	// The parents now have the values that used to belong to the children.
	parent_subpop_size_ = child_subpop_size_;
	parent_sex_ratio_ = child_sex_ratio_;
	parent_first_male_index_ = child_first_male_index_;
	
	// mark the child generation as invalid, until it is generated
	child_generation_valid_ = false;
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFit(false);	// false means generate only new children, not new parents
}

bool Subpopulation::ContainsGenome(Genome *p_genome)
{
	if (parent_genomes_.size())
	{
		Genome *parent_genomes_front = &parent_genomes_.front();
		Genome *parent_genomes_back = &parent_genomes_.back();
		
		if ((p_genome >= parent_genomes_front) && (p_genome <= parent_genomes_back))
			return true;
	}
	
	if (child_genomes_.size())
	{
		Genome *child_genomes_front = &child_genomes_.front();
		Genome *child_genomes_back = &child_genomes_.back();
		
		if ((p_genome >= child_genomes_front) && (p_genome <= child_genomes_back))
			return true;
	}
	
	return false;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

const EidosObjectClass *Subpopulation::Class(void) const
{
	return gSLiM_Subpopulation_Class;
}

void Subpopulation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<p" << subpopulation_id_ << ">";
}

EidosValue_SP Subpopulation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:				// ACCELERATED
		{
			if (!cached_value_subpop_id_)
				cached_value_subpop_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpopulation_id_));
			return cached_value_subpop_id_;
		}
		case gID_firstMaleIndex:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_));
		case gID_genomes:
		{
			if (child_generation_valid_)
			{
				if (!cached_child_genomes_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class);
					cached_child_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter = child_genomes_.begin(); genome_iter != child_genomes_.end(); genome_iter++)
						vec->PushObjectElement(&(*genome_iter));		// operator * can be overloaded by the iterator
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = (EidosValue_Object_vector *)cached_child_genomes_value_.get();
					const std::vector<EidosObjectElement *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)child_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(child_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_child_genomes_value_." << eidos_terminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_child_genomes_value_." << eidos_terminate();
				}
				*/
				
				return cached_child_genomes_value_;
			}
			else
			{
				if (!cached_parent_genomes_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class);
					cached_parent_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter = parent_genomes_.begin(); genome_iter != parent_genomes_.end(); genome_iter++)
						vec->PushObjectElement(&(*genome_iter));		// operator * can be overloaded by the iterator
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = (EidosValue_Object_vector *)cached_parent_genomes_value_.get();
					const std::vector<EidosObjectElement *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)parent_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(parent_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_parent_genomes_value_." << eidos_terminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_parent_genomes_value_." << eidos_terminate();
				}
				*/
				
				return cached_parent_genomes_value_;
			}
		}
		case gID_individuals:
		{
			if (child_generation_valid_)
			{
				slim_popsize_t subpop_size = child_subpop_size_;
				
				// Check for an outdated cache and detach from it
				// BCH 7 November 2016: This probably never occurs any more; we reset() in
				// GenerateChildrenToFit() now, pre-emptively.  See the comment there.
				if (cached_child_individuals_value_ && (cached_child_individuals_value_->Count() != subpop_size))
					cached_child_individuals_value_.reset();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_child_individuals_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
					cached_child_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->PushObjectElement(&child_individuals_[individual_index]);
				}
				
				return cached_child_individuals_value_;
			}
			else
			{
				slim_popsize_t subpop_size = parent_subpop_size_;
				
				// Check for an outdated cache and detach from it
				// BCH 7 November 2016: This probably never occurs any more; we reset() in
				// GenerateChildrenToFit() now, pre-emptively.  See the comment there.
				if (cached_parent_individuals_value_ && (cached_parent_individuals_value_->Count() != subpop_size))
					cached_parent_individuals_value_.reset();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_parent_individuals_value_)
				{
					EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
					cached_parent_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->PushObjectElement(&parent_individuals_[individual_index]);
				}
				
				return cached_parent_individuals_value_;
			}
		}
		case gID_immigrantSubpopIDs:
		{
			EidosValue_Int_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
				vec->PushInt(migrant_pair->first);
			
			return result_SP;
		}
		case gID_immigrantSubpopFractions:
		{
			EidosValue_Float_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
				vec->PushFloat(migrant_pair->second);
			
			return result_SP;
		}
		case gID_selfingRate:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selfing_fraction_));
		case gID_cloningRate:
			if (sex_enabled_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{female_clone_fraction_, male_clone_fraction_});
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(female_clone_fraction_));
		case gID_sexRatio:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_));
		case gID_spatialBounds:
		{
			SLiMSim &sim = population_.sim_;
			int dimensionality = sim.SpatialDimensionality();
			
			switch (dimensionality)
			{
				case 0: return gStaticEidosValue_Float_ZeroVec;
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_x1_});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_x1_, bounds_y1_});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_z0_, bounds_x1_, bounds_y1_, bounds_z1_});
			}
		}
		case gID_individualCount:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_));
			
			// variables
		case gID_tag:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t Subpopulation::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_id:				return subpopulation_id_;
		case gID_firstMaleIndex:	return (child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_);
		case gID_individualCount:	return (child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_);
		case gID_tag:				return tag_value_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

double Subpopulation::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_selfingRate:		return selfing_fraction_;
		case gID_sexRatio:			return (child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_);
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Float(p_property_id);
	}
}


void Subpopulation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Subpopulation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	EidosValue *arg3_value = ((p_argument_count >= 4) ? p_arguments[3].get() : nullptr);
	EidosValue *arg4_value = ((p_argument_count >= 5) ? p_arguments[4].get() : nullptr);
	EidosValue *arg5_value = ((p_argument_count >= 6) ? p_arguments[5].get() : nullptr);
	
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (void)setMigrationRates(object sourceSubpops, numeric rates)
			//
#pragma mark -setMigrationRates()
			
		case gID_setMigrationRates:
		{
			int source_subpops_count = arg0_value->Count();
			int rates_count = arg1_value->Count();
			std::vector<slim_objectid_t> subpops_seen;
			
			if (source_subpops_count != rates_count)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setMigrationRates() requires sourceSubpops and rates to be equal in size." << eidos_terminate();
			
			for (int value_index = 0; value_index < source_subpops_count; ++value_index)
			{
				EidosObjectElement *source_subpop = nullptr;
				
				if (arg0_value->Type() == EidosValueType::kValueInt)
				{
					slim_objectid_t subpop_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(value_index, nullptr));
					auto found_subpop_pair = population_.find(subpop_id);
					
					if (found_subpop_pair == population_.end())
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setMigrationRates() subpopulation p" << subpop_id << " not defined." << eidos_terminate();
					
					source_subpop = found_subpop_pair->second;
				}
				else
				{
					source_subpop = arg0_value->ObjectElementAtIndex(value_index, nullptr);
				}
				
				slim_objectid_t source_subpop_id = ((Subpopulation *)(source_subpop))->subpopulation_id_;
				
				if (source_subpop_id == subpopulation_id_)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setMigrationRates() does not allow migration to be self-referential (originating within the destination subpopulation)." << eidos_terminate();
				if (std::find(subpops_seen.begin(), subpops_seen.end(), source_subpop_id) != subpops_seen.end())
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setMigrationRates() two rates set for subpopulation p" << source_subpop_id << "." << eidos_terminate();
				
				double migrant_fraction = arg1_value->FloatAtIndex(value_index, nullptr);
				
				population_.SetMigration(*this, source_subpop_id, migrant_fraction);
				subpops_seen.emplace_back(source_subpop_id);
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	– (logical$)pointInBounds(float point)
			//
#pragma mark -pointInBounds()
			
		case gID_pointInBounds:
		{
			SLiMSim &sim = population_.sim_;
			
			int dimensionality = sim.SpatialDimensionality();
			int value_count = arg0_value->Count();
			
			if (dimensionality == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointInBounds() cannot be called in non-spatial simulations." << eidos_terminate();
			if (value_count < dimensionality)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointInBounds() requires at least as many coordinates as the spatial dimensionality of the simulation." << eidos_terminate();
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					return ((x >= bounds_x0_) && (x <= bounds_x1_))
						? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				}
				case 2:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					double y = arg0_value->FloatAtIndex(1, nullptr);
					return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_))
						? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				}
				case 3:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					double y = arg0_value->FloatAtIndex(1, nullptr);
					double z = arg0_value->FloatAtIndex(2, nullptr);
					return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_) && (z >= bounds_z0_) && (z <= bounds_z1_))
						? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	– (float)pointReflected(float point)
			//
#pragma mark -pointReflected()
			
		case gID_pointReflected:
		{
			SLiMSim &sim = population_.sim_;
			
			int dimensionality = sim.SpatialDimensionality();
			int value_count = arg0_value->Count();
			
			if (dimensionality == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointReflected() cannot be called in non-spatial simulations." << eidos_terminate();
			if (value_count < dimensionality)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointReflected() requires at least as many coordinates as the spatial dimensionality of the simulation." << eidos_terminate();
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					
					while (true)
					{
						if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
						else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
						else break;
					}
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
				}
				case 2:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					double y = arg0_value->FloatAtIndex(1, nullptr);
					
					while (true)
					{
						if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
						else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
						else break;
					}
					
					while (true)
					{
						if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
						else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
						else break;
					}
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
				}
				case 3:
				{
					double x = arg0_value->FloatAtIndex(0, nullptr);
					double y = arg0_value->FloatAtIndex(1, nullptr);
					double z = arg0_value->FloatAtIndex(2, nullptr);
					
					while (true)
					{
						if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
						else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
						else break;
					}
					
					while (true)
					{
						if (y < bounds_y0_) y = bounds_y0_ + (bounds_y0_ - y);
						else if (y > bounds_y1_) y = bounds_y1_ - (y - bounds_y1_);
						else break;
					}
					
					while (true)
					{
						if (z < bounds_z0_) z = bounds_z0_ + (bounds_z0_ - z);
						else if (z > bounds_z1_) z = bounds_z1_ - (z - bounds_z1_);
						else break;
					}
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	– (float)pointStopped(float point)
			//
#pragma mark -pointStopped()
			
		case gID_pointStopped:
		{
			SLiMSim &sim = population_.sim_;
			
			int dimensionality = sim.SpatialDimensionality();
			int value_count = arg0_value->Count();
			
			if (dimensionality == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointStopped() cannot be called in non-spatial simulations." << eidos_terminate();
			if (value_count < dimensionality)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointStopped() requires at least as many coordinates as the spatial dimensionality of the simulation." << eidos_terminate();
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
				}
				case 2:
				{
					double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
					double y = std::max(bounds_y0_, std::min(bounds_y1_, arg0_value->FloatAtIndex(1, nullptr)));
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
				}
				case 3:
				{
					double x = std::max(bounds_x0_, std::min(bounds_x1_, arg0_value->FloatAtIndex(0, nullptr)));
					double y = std::max(bounds_y0_, std::min(bounds_y1_, arg0_value->FloatAtIndex(1, nullptr)));
					double z = std::max(bounds_z0_, std::min(bounds_z1_, arg0_value->FloatAtIndex(2, nullptr)));
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	– (float)pointUniform(void)
			//
#pragma mark -pointUniform()
			
		case gID_pointUniform:
		{
			SLiMSim &sim = population_.sim_;
			
			int dimensionality = sim.SpatialDimensionality();
			
			if (dimensionality == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): pointUniform() cannot be called in non-spatial simulations." << eidos_terminate();
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
				}
				case 2:
				{
					double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
					double y = gsl_rng_uniform(gEidos_rng) * (bounds_y1_ - bounds_y0_) + bounds_y0_;
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y});
				}
				case 3:
				{
					double x = gsl_rng_uniform(gEidos_rng) * (bounds_x1_ - bounds_x0_) + bounds_x0_;
					double y = gsl_rng_uniform(gEidos_rng) * (bounds_y1_ - bounds_y0_) + bounds_y0_;
					double z = gsl_rng_uniform(gEidos_rng) * (bounds_z1_ - bounds_z0_) + bounds_z0_;
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{x, y, z});
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	- (void)setCloningRate(numeric rate)
			//
#pragma mark -setCloningRate()
			
		case gID_setCloningRate:
		{
			int value_count = arg0_value->Count();
			
			if (sex_enabled_)
			{
				// SEX ONLY: either one or two values may be specified; if two, it is female at 0, male at 1
				if ((value_count < 1) || (value_count > 2))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setCloningRate() requires a rate vector containing either one or two values, in sexual simulations." << eidos_terminate();
				
				double female_cloning_fraction = arg0_value->FloatAtIndex(0, nullptr);
				double male_cloning_fraction = (value_count == 2) ? arg0_value->FloatAtIndex(1, nullptr) : female_cloning_fraction;
				
				if (female_cloning_fraction < 0.0 || female_cloning_fraction > 1.0)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setCloningRate() requires cloning fractions within [0,1] (" << female_cloning_fraction << " supplied)." << eidos_terminate();
				if (male_cloning_fraction < 0.0 || male_cloning_fraction > 1.0)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setCloningRate() requires cloning fractions within [0,1] (" << male_cloning_fraction << " supplied)." << eidos_terminate();
				
				female_clone_fraction_ = female_cloning_fraction;
				male_clone_fraction_ = male_cloning_fraction;
			}
			else
			{
				// ASEX ONLY: only one value may be specified
				if (value_count != 1)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setCloningRate() requires a rate vector containing exactly one value, in asexual simulations.." << eidos_terminate();
				
				double cloning_fraction = arg0_value->FloatAtIndex(0, nullptr);
				
				if (cloning_fraction < 0.0 || cloning_fraction > 1.0)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setCloningRate() requires cloning fractions within [0,1] (" << cloning_fraction << " supplied)." << eidos_terminate();
				
				female_clone_fraction_ = cloning_fraction;
				male_clone_fraction_ = cloning_fraction;
			}
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	- (void)setSelfingRate(numeric$ rate)
			//
#pragma mark -setSelfingRate()
			
		case gID_setSelfingRate:
		{
			double selfing_fraction = arg0_value->FloatAtIndex(0, nullptr);
			
			if ((selfing_fraction != 0.0) && sex_enabled_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSelfingRate() is limited to the hermaphroditic case, and cannot be called in sexual simulations." << eidos_terminate();
			
			if (selfing_fraction < 0.0 || selfing_fraction > 1.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSelfingRate() requires a selfing fraction within [0,1] (" << selfing_fraction << " supplied)." << eidos_terminate();
			
			selfing_fraction_ = selfing_fraction;
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	- (void)setSexRatio(float$ sexRatio)
			//
#pragma mark -setSexRatio()
			
		case gID_setSexRatio:
		{
			// SetSexRatio() can only be called when the child generation has not yet been generated.  It sets the sex ratio on the child generation,
			// and then that sex ratio takes effect when the children are generated from the parents in EvolveSubpopulation().
			if (child_generation_valid_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSexRatio() called when the child generation was valid." << eidos_terminate();
			
			// SEX ONLY
			if (!sex_enabled_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSexRatio() is limited to the sexual case, and cannot be called in asexual simulations." << eidos_terminate();
			
			double sex_ratio = arg0_value->FloatAtIndex(0, nullptr);
			
			if (sex_ratio < 0.0 || sex_ratio > 1.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSexRatio() requires a sex ratio within [0,1] (" << sex_ratio << " supplied)." << eidos_terminate();
			
			// After we change the subpop sex ratio, we need to generate new children genomes to fit the new requirements
			child_sex_ratio_ = sex_ratio;
			GenerateChildrenToFit(false);	// false means generate only new children, not new parents
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	– (void)setSpatialBounds(float position)
			//
#pragma mark -setSpatialBounds()
			
		case gID_setSpatialBounds:
		{
			SLiMSim &sim = population_.sim_;
			
			int dimensionality = sim.SpatialDimensionality();
			int value_count = arg0_value->Count();
			
			if (dimensionality == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSpatialBounds() cannot be called in non-spatial simulations." << eidos_terminate();
			
			if (value_count != dimensionality * 2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSpatialBounds() requires twice as many coordinates as the spatial dimensionality of the simulation." << eidos_terminate();
			
			bool bad_bounds = false;
			
			switch (dimensionality)
			{
				case 1:
					bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(1, nullptr);
					
					if (bounds_x1_ <= bounds_x0_)
						bad_bounds = true;
					
					break;
				case 2:
					bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(2, nullptr);
					bounds_y0_ = arg0_value->FloatAtIndex(1, nullptr);	bounds_y1_ = arg0_value->FloatAtIndex(3, nullptr);
					
					if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_))
						bad_bounds = true;
					
					break;
				case 3:
					bounds_x0_ = arg0_value->FloatAtIndex(0, nullptr);	bounds_x1_ = arg0_value->FloatAtIndex(3, nullptr);
					bounds_y0_ = arg0_value->FloatAtIndex(1, nullptr);	bounds_y1_ = arg0_value->FloatAtIndex(4, nullptr);
					bounds_z0_ = arg0_value->FloatAtIndex(2, nullptr);	bounds_z1_ = arg0_value->FloatAtIndex(5, nullptr);
					
					if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_) || (bounds_z1_ <= bounds_z0_))
						bad_bounds = true;
					
					break;
			}
			
			if (bad_bounds)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): setSpatialBounds() requires min coordinates to be less than max coordinates." << eidos_terminate();
			
			return gStaticEidosValueNULLInvisible;
		}			
			
			
			//
			//	*********************	- (void)setSubpopulationSize(integer$ size)
			//
#pragma mark -setSubpopulationSize()
			
		case gID_setSubpopulationSize:
		{
			slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
			
			population_.SetSize(*this, subpop_size);
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	- (float)cachedFitness(Ni indices)
			//
#pragma mark -cachedFitness()
			
		case gID_cachedFitness:
		{
			if (child_generation_valid_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): cachedFitness() may only be called when the parental generation is active (before or during offspring generation)." << eidos_terminate();
			if (cached_fitness_size_ == 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): cachedFitness() may not be called while fitness values are being calculated, or before the first time they are calculated." << eidos_terminate();
			
			bool do_all_indices = (arg0_value->Type() == EidosValueType::kValueNULL);
			slim_popsize_t index_count = (do_all_indices ? parent_subpop_size_ : SLiMCastToPopsizeTypeOrRaise(arg0_value->Count()));
			
			if (index_count == 1)
			{
				slim_popsize_t index = 0;
				
				if (!do_all_indices)
				{
					index = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
					
					if (index >= cached_fitness_size_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): cachedFitness() index " << index << " out of range." << eidos_terminate();
				}
				
				double fitness = cached_parental_fitness_[index];
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness));
			}
			else
			{
				EidosValue_Float_vector *float_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(index_count);
				EidosValue_SP result_SP = EidosValue_SP(float_return);
				
				for (slim_popsize_t value_index = 0; value_index < index_count; value_index++)
				{
					slim_popsize_t index = value_index;
					
					if (!do_all_indices)
					{
						index = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(value_index, nullptr));
						
						if (index >= cached_fitness_size_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): cachedFitness() index " << index << " out of range." << eidos_terminate();
					}
					
					double fitness = cached_parental_fitness_[index];
					
					float_return->PushFloat(fitness);
				}
				
				return result_SP;
			}
		}
			
			
			//
			//	*********************	– (void)outputMSSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
			//	*********************	– (void)outputSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
			//	*********************	– (void)outputVCFSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [logical$ outputMultiallelics = T], [Ns$ filePath = NULL], [logical$ append=F])
			//
#pragma mark -outputMSSample()
#pragma mark -outputVCFSample()
#pragma mark -outputSample()
			
		case gID_outputMSSample:
		case gID_outputVCFSample:
		case gID_outputSample:
		{
			std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
			SLiMSim &sim = population_.sim_;
			
			if ((sim.GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim.warned_early_output_))
			{
				output_stream << "#WARNING (Subpopulation::ExecuteInstanceMethod): " << StringForEidosGlobalStringID(p_method_id) << "() should probably not be called from an early() event; the output will reflect state at the beginning of the generation, not the end." << std::endl;
				sim.warned_early_output_ = true;
			}
			
			slim_popsize_t sample_size = SLiMCastToPopsizeTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
			
			bool replace = arg1_value->LogicalAtIndex(0, nullptr);
			
			IndividualSex requested_sex;
			
			string sex_string = arg2_value->StringAtIndex(0, nullptr);
			
			if (sex_string.compare("M") == 0)
				requested_sex = IndividualSex::kMale;
			else if (sex_string.compare("F") == 0)
				requested_sex = IndividualSex::kFemale;
			else if (sex_string.compare("*") == 0)
				requested_sex = IndividualSex::kUnspecified;
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): " << StringForEidosGlobalStringID(p_method_id) << "() requested sex \"" << sex_string << "\" unsupported." << eidos_terminate();
			
			if (!sim.SexEnabled() && requested_sex != IndividualSex::kUnspecified)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteInstanceMethod): " << StringForEidosGlobalStringID(p_method_id) << "() requested sex is not legal in a non-sexual simulation." << eidos_terminate();
			
			bool output_multiallelics = true;
			
			if (p_method_id == gID_outputVCFSample)
				output_multiallelics = arg3_value->LogicalAtIndex(0, nullptr);
			
			// Figure out the right output stream
			std::ofstream outfile;
			bool has_file = false;
			string outfile_path;
			EidosValue *file_arg = ((p_method_id == gID_outputVCFSample) ? arg4_value : arg3_value);
			EidosValue *append_arg = ((p_method_id == gID_outputVCFSample) ? arg5_value : arg4_value);
			
			if (file_arg->Type() != EidosValueType::kValueNULL)
			{
				outfile_path = EidosResolvedPath(file_arg->StringAtIndex(0, nullptr));
				bool append = append_arg->LogicalAtIndex(0, nullptr);
				
				outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
				has_file = true;
				
				if (!outfile.is_open())
					EIDOS_TERMINATION << "ERROR (SLiMSim::ExecuteInstanceMethod): outputFixedMutations() could not open "<< outfile_path << "." << eidos_terminate();
			}
			
			std::ostream &out = *(has_file ? dynamic_cast<std::ostream *>(&outfile) : dynamic_cast<std::ostream *>(&output_stream));
			
			if (!has_file || (p_method_id == gID_outputSample))
			{
				// Output header line
				out << "#OUT: " << sim.Generation() << " S";
				
				if (p_method_id == gID_outputSample)
					out << "S";
				else if (p_method_id == gID_outputMSSample)
					out << "M";
				else if (p_method_id == gID_outputVCFSample)
					out << "V";
				
				out << " p" << subpopulation_id_ << " " << sample_size;
				
				if (sim.SexEnabled())
					out << " " << requested_sex;
				
				if (has_file)
					out << " " << outfile_path;
				
				out << endl;
			}
			
			// Call out to produce the actual sample
			if (p_method_id == gID_outputSample)
				population_.PrintSample_slim(out, *this, sample_size, replace, requested_sex);
			else if (p_method_id == gID_outputMSSample)
				population_.PrintSample_ms(out, *this, sample_size, replace, requested_sex, sim.TheChromosome());
			else if (p_method_id == gID_outputVCFSample)
				population_.PrintSample_vcf(out, *this, sample_size, replace, requested_sex, output_multiallelics);
			
			if (has_file)
				outfile.close(); 
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
			return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Subpopulation_Class
//
#pragma mark -
#pragma mark Subpopulation_Class

class Subpopulation_Class : public SLiMEidosDictionary_Class
{
public:
	Subpopulation_Class(const Subpopulation_Class &p_original) = delete;	// no copy-construct
	Subpopulation_Class& operator=(const Subpopulation_Class&) = delete;	// no copying
	
	Subpopulation_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Subpopulation_Class = new Subpopulation_Class;


Subpopulation_Class::Subpopulation_Class(void)
{
}

const std::string &Subpopulation_Class::ElementType(void) const
{
	return gStr_Subpopulation;
}

const std::vector<const EidosPropertySignature *> *Subpopulation_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_firstMaleIndex));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_individuals));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_immigrantSubpopIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_immigrantSubpopFractions));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_selfingRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_cloningRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sexRatio));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_spatialBounds));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_individualCount));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Subpopulation_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *firstMaleIndexSig = nullptr;
	static EidosPropertySignature *genomesSig = nullptr;
	static EidosPropertySignature *individualsSig = nullptr;
	static EidosPropertySignature *immigrantSubpopIDsSig = nullptr;
	static EidosPropertySignature *immigrantSubpopFractionsSig = nullptr;
	static EidosPropertySignature *selfingRateSig = nullptr;
	static EidosPropertySignature *cloningRateSig = nullptr;
	static EidosPropertySignature *sexRatioSig = nullptr;
	static EidosPropertySignature *spatialBoundsSig = nullptr;
	static EidosPropertySignature *sizeSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =							(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,							gID_id,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		firstMaleIndexSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_firstMaleIndex,				gID_firstMaleIndex,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		genomesSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,						gID_genomes,					true,	kEidosValueMaskObject, gSLiM_Genome_Class));
		individualsSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_individuals,					gID_individuals,				true,	kEidosValueMaskObject, gSLiM_Individual_Class));
		immigrantSubpopIDsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopIDs,			gID_immigrantSubpopIDs,			true,	kEidosValueMaskInt));
		immigrantSubpopFractionsSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopFractions,	gID_immigrantSubpopFractions,	true,	kEidosValueMaskFloat));
		selfingRateSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_selfingRate,					gID_selfingRate,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		cloningRateSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_cloningRate,					gID_cloningRate,				true,	kEidosValueMaskFloat));
		sexRatioSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_sexRatio,					gID_sexRatio,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		spatialBoundsSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialBounds,				gID_spatialBounds,				true,	kEidosValueMaskFloat));
		sizeSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_individualCount,				gID_individualCount,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,							gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:						return idSig;
		case gID_firstMaleIndex:			return firstMaleIndexSig;
		case gID_genomes:					return genomesSig;
		case gID_individuals:				return individualsSig;
		case gID_immigrantSubpopIDs:		return immigrantSubpopIDsSig;
		case gID_immigrantSubpopFractions:	return immigrantSubpopFractionsSig;
		case gID_selfingRate:				return selfingRateSig;
		case gID_cloningRate:				return cloningRateSig;
		case gID_sexRatio:					return sexRatioSig;
		case gID_spatialBounds:				return spatialBoundsSig;
		case gID_individualCount:			return sizeSig;
		case gID_tag:						return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Subpopulation_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setMigrationRates));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointInBounds));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointReflected));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointStopped));
		methods->emplace_back(SignatureForMethodOrRaise(gID_pointUniform));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setCloningRate));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSelfingRate));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSexRatio));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSpatialBounds));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSubpopulationSize));
		methods->emplace_back(SignatureForMethodOrRaise(gID_cachedFitness));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputMSSample));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputVCFSample));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputSample));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Subpopulation_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	// Signatures are all preallocated, for speed
	static EidosInstanceMethodSignature *setMigrationRatesSig = nullptr;
	static EidosInstanceMethodSignature *pointInBoundsSig = nullptr;
	static EidosInstanceMethodSignature *pointReflectedSig = nullptr;
	static EidosInstanceMethodSignature *pointStoppedSig = nullptr;
	static EidosInstanceMethodSignature *pointUniformSig = nullptr;
	static EidosInstanceMethodSignature *setCloningRateSig = nullptr;
	static EidosInstanceMethodSignature *setSelfingRateSig = nullptr;
	static EidosInstanceMethodSignature *setSexRatioSig = nullptr;
	static EidosInstanceMethodSignature *setSpatialBoundsSig = nullptr;
	static EidosInstanceMethodSignature *setSubpopulationSizeSig = nullptr;
	static EidosInstanceMethodSignature *cachedFitnessSig = nullptr;
	static EidosInstanceMethodSignature *outputMSSampleSig = nullptr;
	static EidosInstanceMethodSignature *outputVCFSampleSig = nullptr;
	static EidosInstanceMethodSignature *outputSampleSig = nullptr;
	
	if (!setMigrationRatesSig)
	{
		setMigrationRatesSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMigrationRates, kEidosValueMaskNULL))->AddIntObject("sourceSubpops", gSLiM_Subpopulation_Class)->AddNumeric("rates");
		pointInBoundsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointInBounds, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddFloat("point");
		pointReflectedSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointReflected, kEidosValueMaskFloat))->AddFloat("point");
		pointStoppedSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointStopped, kEidosValueMaskFloat))->AddFloat("point");
		pointUniformSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniform, kEidosValueMaskFloat));
		setCloningRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setCloningRate, kEidosValueMaskNULL))->AddNumeric("rate");
		setSelfingRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelfingRate, kEidosValueMaskNULL))->AddNumeric_S("rate");
		setSexRatioSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSexRatio, kEidosValueMaskNULL))->AddFloat_S("sexRatio");
		setSpatialBoundsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialBounds, kEidosValueMaskNULL))->AddFloat("bounds");
		setSubpopulationSizeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSubpopulationSize, kEidosValueMaskNULL))->AddInt_S("size");
		cachedFitnessSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_cachedFitness, kEidosValueMaskFloat))->AddInt_N("indices");
		outputMSSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMSSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
		outputVCFSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputVCFSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
		outputSampleSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputSample, kEidosValueMaskNULL))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_setMigrationRates:		return setMigrationRatesSig;
		case gID_pointInBounds:			return pointInBoundsSig;
		case gID_pointReflected:		return pointReflectedSig;
		case gID_pointStopped:			return pointStoppedSig;
		case gID_pointUniform:			return pointUniformSig;
		case gID_setCloningRate:		return setCloningRateSig;
		case gID_setSelfingRate:		return setSelfingRateSig;
		case gID_setSexRatio:			return setSexRatioSig;
		case gID_setSpatialBounds:		return setSpatialBoundsSig;
		case gID_setSubpopulationSize:	return setSubpopulationSizeSig;
		case gID_cachedFitness:			return cachedFitnessSig;
		case gID_outputMSSample:		return outputMSSampleSig;
		case gID_outputVCFSample:		return outputVCFSampleSig;
		case gID_outputSample:			return outputSampleSig;
			
			// all others, including gID_none
		default:
			return SLiMEidosDictionary_Class::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Subpopulation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}




































































