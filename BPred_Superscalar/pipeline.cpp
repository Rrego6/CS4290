/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Aditya Somani 
 * Date         : 8th February 2015
 * Description  : Superscalar Pipeline for Lab 2 CS 4290/6290
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>

extern int32_t PIPE_WIDTH;
extern int32_t ENABLE_MEM_FWD;
extern int32_t ENABLE_EXE_FWD;
extern int32_t BPRED_POLICY;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Op
 **********************************************************************/

void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
    uint8_t bytes_read = 0;
    bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);

    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
      fetch_op->valid=false;
      p->halt_op_id=p->op_id_tracker;
      return;
    }

    // got an instruction ... hooray!
    fetch_op->valid=true;
    fetch_op->stall=false;
    fetch_op->is_mispred_cbr=false;
    p->op_id_tracker++;
    fetch_op->op_id=p->op_id_tracker;
    
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->tr_file = tr_file_in;
    p->halt_op_id = ((uint64_t)-1) - 3;           

    // Allocated Branch Predictor
    if(BPRED_POLICY){
      p->b_pred = new BPRED(BPRED_POLICY);
    }

    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;

    uint8_t latch_type_i = 0;   // Iterates over Latch Types
    uint8_t width_i      = 0;   // Iterates over Pipeline Width
    for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
        switch(latch_type_i) {
            case 0:
                printf(" FE: ");
                break;
            case 1:
                printf(" ID: ");
                break;
            case 2:
                printf(" EX: ");
                break;
            case 3:
                printf(" MEM: ");
                break;
            default:
                printf(" ---- ");
        }
    }
    printf("\n");
    for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
        for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
            if(p->pipe_latch[latch_type_i][width_i].valid == true) {
	      printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
            } else {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");

}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);
}
/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(p->pipe_latch[MEM_LATCH][ii].valid){
		p->stat_retired_inst++;
		if(p->pipe_latch[MEM_LATCH][ii].op_id >= p->halt_op_id){
			p->halt=true;
		}
		if(p->fetch_cbr_stall && p->pipe_latch[MEM_LATCH][ii].is_mispred_cbr)
		{
			p->fetch_cbr_stall = false;
		}
	}
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
	if(p->pipe_latch[EX_LATCH][ii].stall)
	{
		p->pipe_latch[EX_LATCH][ii].valid = false;
	}
	else
	{
		p->pipe_latch[ID_LATCH][ii].valid = false;
	}
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_ID(Pipeline *p){
int ii;
  for(ii=0; ii<2*PIPE_WIDTH; ii++){
    if(!p->pipe_latch[ID_LATCH][ii].stall && ii<PIPE_WIDTH)
	{
		p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
	}
    if(ENABLE_MEM_FWD & ENABLE_EXE_FWD)
	{
		p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = false;
		for(int jj=0; jj<PIPE_WIDTH; jj++)
		{
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_needed && p->pipe_latch[EX_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[EX_LATCH][jj].valid && p->pipe_latch[EX_LATCH][jj].tr_entry.op_type == OP_LD)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_reg == p->pipe_latch[EX_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_needed && p->pipe_latch[EX_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[EX_LATCH][jj].valid && p->pipe_latch[EX_LATCH][jj].tr_entry.op_type == OP_LD)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_reg == p->pipe_latch[EX_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.cc_read && p->pipe_latch[EX_LATCH][jj].tr_entry.cc_write && p->pipe_latch[EX_LATCH][jj].valid && p->pipe_latch[EX_LATCH][jj].tr_entry.op_type == OP_LD)
			{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}		
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_needed && p->pipe_latch[ID_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_reg == p->pipe_latch[ID_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_needed && p->pipe_latch[ID_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_reg == p->pipe_latch[ID_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.cc_read && p->pipe_latch[ID_LATCH][jj].tr_entry.cc_write && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id && p->pipe_latch[ID_LATCH][jj].stall)
			{
				p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}
		}
    }
	
	if(!ENABLE_MEM_FWD & !ENABLE_EXE_FWD)
	{
		p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = false;
		for(int jj=0; jj<PIPE_WIDTH; jj++)
		{
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_needed && p->pipe_latch[EX_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[EX_LATCH][jj].valid)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_reg == p->pipe_latch[EX_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_needed && p->pipe_latch[EX_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[EX_LATCH][jj].valid)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_reg == p->pipe_latch[EX_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_needed && p->pipe_latch[MEM_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[MEM_LATCH][jj].valid)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_reg == p->pipe_latch[MEM_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_needed && p->pipe_latch[MEM_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[MEM_LATCH][jj].valid)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_reg == p->pipe_latch[MEM_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.cc_read && p->pipe_latch[EX_LATCH][jj].tr_entry.cc_write && p->pipe_latch[EX_LATCH][jj].valid)
			{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.cc_read && p->pipe_latch[MEM_LATCH][jj].tr_entry.cc_write && p->pipe_latch[MEM_LATCH][jj].valid)
			{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}		
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_needed && p->pipe_latch[ID_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src1_reg == p->pipe_latch[ID_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_needed && p->pipe_latch[ID_LATCH][jj].tr_entry.dest_needed && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.src2_reg == p->pipe_latch[ID_LATCH][jj].tr_entry.dest)
				{
					p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
				}
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].tr_entry.cc_read && p->pipe_latch[ID_LATCH][jj].tr_entry.cc_write && p->pipe_latch[ID_LATCH][jj].valid && p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id)
			{
				p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}
			if(p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].op_id > p->pipe_latch[ID_LATCH][jj].op_id && p->pipe_latch[ID_LATCH][jj].stall && p->pipe_latch[ID_LATCH][jj].valid)
			{
				p->pipe_latch[ID_LATCH][ii%PIPE_WIDTH].stall = true;
			}
		}
	}
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(!p->pipe_latch[ID_LATCH][ii].stall && !p->fetch_cbr_stall)
	{
		pipe_get_fetch_op(p, &fetch_op);
	}
	else
	{
		fetch_op.valid = false;
	}
	
    if(BPRED_POLICY){
      pipe_check_bpred(p, &fetch_op);
    }
    
    // copy the op in FE LATCH
	p->pipe_latch[FE_LATCH][ii]=fetch_op;
  }
  
}


//--------------------------------------------------------------------//

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
  
  static BPRED Predictor(BPRED_POLICY);
  if(fetch_op->tr_entry.op_type == OP_CBR && fetch_op->valid)
  {
	  p->b_pred->stat_num_branches++;
	  if (Predictor.GetPrediction(fetch_op->tr_entry.inst_addr) != fetch_op->tr_entry.br_dir)
	  {
		  p->b_pred->stat_num_mispred++;
		  fetch_op->is_mispred_cbr = true;
		  p->fetch_cbr_stall = true;
	  }
  }
}


//--------------------------------------------------------------------//

