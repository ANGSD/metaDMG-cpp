int mod_in[] =  {1649555 , 1401172 ,1582271, 374764, 242716,1793725 ,292451,38298,  63403  ,67357  , 163247  ,328623   ,356150 , 502130, 545877,996989  ,996990,1086724,1169024,1576640, 1769757,1802981,1811974 ,1955118};
int mod_out[]=  {1333996 , 1333996 ,1582270,1914213,1917265,1915309 ,263865,2801 ,1916091,285450,1762941,1916091,157727,1932322,376133,1762939 ,1762946,430531, 1169025,1247960,1769758,1708715,1708715	,1925741};

#include <cassert>
#include <cstdio>
#include <zlib.h>
#include <cstring>
#include <map>
#include <cstdlib>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <vector>
#include <pthread.h>
#include <signal.h>//for catching ctrl+c, allow threads to finish
#include <algorithm>
#include <errno.h>
#include <sys/stat.h>
#include <htslib/bgzf.h>
#include <htslib/kstring.h>

#include "ngsLCA_cli.h"
#include "profile.h"
#include "shared.h"
#include "version.h"

int SIG_COND =1;//if we catch signal then quit program nicely
int VERBOSE =1;
int really_kill =3;

void handler(int s) {
if(VERBOSE)
    fprintf(stderr,"\n\t-> Caught SIGNAL: Will try to exit nicely (no more threads are created.\n\t\t\t  We will wait for the current threads to finish)\n");
  
  if(--really_kill!=3)
  fprintf(stderr,"\n\t-> If you really want ./ngsLCA to exit uncleanly ctrl+c: %d more times\n",really_kill+1);
  fflush(stderr);
  if(!really_kill)
    exit(0);
  VERBOSE=0;
  SIG_COND=0;
}

 //we are threading so we want make a nice signal handler for ctrl+c
void catchkill(){
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handler;
  sigaction(SIGPIPE, &sa, 0);
  sigaction(SIGINT, &sa, 0);  

}

int2int specWeight;// Number of reads that map uniquely to a species.
int2int i2i_missing;//contains counter of missing hits for each taxid that doesnt exists in acc2taxid
char2int c2i_missing;//contains counter of missing hits for each taxid that doesnt exists in acc2taxid

void mod_db(int *in,int *out,int2int &parent, int2char &rank,int2char &name_map){
  for(int i=0;i<24;i++){
    assert(parent.count(out[i])==1);
    parent[in[i]] = parent[out[i]];
    rank[in[i]] = rank[out[i]];
    name_map[in[i]] = strdup("satan");
  }

}
int2int errmap;


 
int2int *bamRefId2tax(bam_hdr_t *hdr,char *acc2taxfile,char *bamfile) { 
fprintf(stderr,"\t-> Starting to extract (acc->taxid) from binary file: \'%s\'\n",acc2taxfile);
fflush(stderr);
int dodump = !fexists3(basename(acc2taxfile),basename(bamfile),".bin");

fprintf(stderr,"check if exists: %d \n", dodump);

time_t t=time(NULL);
BGZF *fp= NULL;
if(dodump)
  fp = getbgzf3(basename(acc2taxfile),basename(bamfile),".bin","wb",4);
 else
   fp =  getbgzf3(basename(acc2taxfile),basename(bamfile),".bin","rb",4);
  //this contains refname(as int) -> taxid
int2int *am= new int2int;


if(dodump){
    char buf[4096];
    int at=0;
    char buf2[4096];
    extern int SIG_COND;
    kstring_t *kstr =(kstring_t *)malloc(sizeof(kstring_t));
    kstr->l=kstr->m = 0;
    kstr->s = NULL;
    BGZF *fp2 = getbgzf(acc2taxfile,"rb",2);
    bgzf_getline(fp2,'\n',kstr);//skip header
    while(SIG_COND&&bgzf_getline(fp2,'\n',kstr)){
if(kstr->l==0)
  break;
//fprintf(stderr,"at: %d = '%s\'\n",at,kstr->s);
      if(!((at++ %100000 ) ))
	if(isatty(fileno(stderr)))
	  fprintf(stderr,"\r\t-> At linenr: %d in \'%s\'      ",at,acc2taxfile);
      char *tok = strtok(kstr->s,"\t\n ");
      char *key =strtok(NULL,"\t\n ");
tok = strtok(NULL,"\t\n ");
int val = atoi(tok);
//fprintf(stderr,"key: %d val: %d\n",key,val);exit(0);
      int valinbam = bam_name2id(hdr,key);
if(valinbam==-1)
  continue;
      bgzf_write(fp,&valinbam,sizeof(int));
      bgzf_write(fp,&val,sizeof(int));
//fprintf(stderr,"key: %s val: %d valinbam:%d\n",key,val,valinbam);

      if(am->find(valinbam)!=am->end())
	fprintf(stderr,"\t-> Duplicate entries found \'%s\'\n",key);
      (*am)[valinbam] = val;
kstr->l =0;
    }
bgzf_close(fp2);
}else{
int valinbam,val;
while(bgzf_read(fp,&valinbam,sizeof(int))){
bgzf_read(fp,&val,sizeof(int));
  (*am)[valinbam] = val;
  }
}

bgzf_close(fp);
fprintf(stderr,"\t-> Number of entries to use from accesion to taxid: %lu, time taken: %.2f sec\n",am->size(),(float)(time(NULL) - t));
  return am;
}

int nodes2root(int taxa,int2int &parent){
  int dist =0;
  while(taxa!=1){
    int2int::iterator it = parent.find(taxa);
    taxa = it->second;
    dist++;
  }
  return dist;
}

int2int dist2root;


int satan(int taxid,int2int &parant){
  int2int::iterator it=dist2root.find(taxid);
  
  return 0;
}



int do_lca(std::vector<int> &taxids,int2int &parent){
  //  fprintf(stderr,"\t-> [%s] with number of taxids: %lu\n",__func__,taxids.size());
  assert(taxids.size()>0);
  if(taxids.size()==1){
    int taxa=taxids[0];
    if(parent.count(taxa)==0){
      fprintf(stderr,"\t-> Problem finding taxaid: %d will skip\n",taxa);
      taxids.clear();
      return -1;
    }

    taxids.clear();
    return taxa;
  }

  int2int counter;
  for(int i=0;i<taxids.size();i++){
    int taxa = taxids[i];
    while(1){
      //      fprintf(stderr,"taxa:%d\n",taxa);
      int2int::iterator it=counter.find(taxa);
      if(it==counter.end()){
	//	fprintf(stderr,"taxa: %d is new will plugin\n",taxa);
	counter[taxa]=1;
      }else
	it->second = it->second+1;
      it = parent.find(taxa);

      if(it==parent.end()){
	int2int::iterator it=errmap.find(taxa);
	if(it==errmap.end()){
	  fprintf(stderr,"\t-> Problem finding parent of :%d\n",taxa);
	  
	  errmap[taxa] = 1;
	}else
	  it->second = it->second +1;
	taxids.clear();
	return -1;
      }
      
      if(taxa==it->second)//<- catch root
	break;
      taxa=it->second;
    }

  }
  //  fprintf(stderr,"counter.size():%lu\n",counter.size());
  //now counts contain how many time a node is traversed to the root
  int2int dist2root;
  for(int2int::iterator it=counter.begin();it!=counter.end();it++)
    if(it->second==taxids.size())
      dist2root[nodes2root(it->first,parent)] = it->first;
  for(int2int::iterator it=dist2root.begin();0&&it!=dist2root.end();it++)
    fprintf(stderr,"%d\t->%d\n",it->first,it->second);
  taxids.clear();
  if(!dist2root.empty())
    return (--dist2root.end())->second;
  else{
    fprintf(stderr,"\t-> Happens\n");
    return -1;
  }
}

void print_chain1(FILE *fp,int taxa,int2char &rank,int2char &name_map){
  int2char::iterator it1=name_map.find(taxa);
  int2char::iterator it2=rank.find(taxa);
  if(it1==name_map.end()){
    fprintf(stderr,"\t-> Problem finding taxaid:%d\n",taxa);
  }
  assert(it2!=rank.end());
  if(it1==name_map.end()||it2==rank.end()){
    fprintf(stderr,"taxa: %d %s doesnt exists will exit\n",taxa,it1->second);
    exit(0);
  }
  fprintf(fp,"\t%d:%s:\"%s\"",taxa,it1->second,it2->second);
  
}

void print_rank(FILE *fp, int taxa, int2char &rank){
  int2char::iterator it2=rank.find(taxa);
  assert(it2!=rank.end());
  fprintf(stderr,"taxa: %d rank %s\n",taxa,it2->second);
}

int get_level(const char *name,int norank2species){
  if(strcmp(name,"subspecies")==0) return 1;
  else  if(strcmp(name,"species")==0) return 2;
  else  if(strcmp(name,"genus")==0) return 3;
  else  if(strcmp(name,"family")==0) return 4;
  else  if(strcmp(name,"no rank")==0){
    //    fprintf(stderr,"is norank: \n");
    if(norank2species==1)
      return 2;
    else
      return 5;
  }
  else{
    //fprintf(stderr,"Unknown level: \'%s\'\n",name);
    
    return -1;
  }
}


int correct_rank(char *filt, int taxa, int2char &rank,int norank2species){
  int2char::iterator it2=rank.find(taxa);
  assert(it2!=rank.end());

  int en = get_level(filt,norank2species);
  int to = get_level(it2->second,norank2species);

  if(en==-1||to==-1)
    return 0;
  
  if(to<=en)
    return 1;
  else
    return 0;
}

void print_chain(FILE *fp,int taxa,int2int &parent,int2char &rank,int2char &name_map){

    while(1){
      print_chain1(fp,taxa,rank,name_map);
      int2int::iterator it = parent.find(taxa);
      assert(it!=parent.end());
      if(taxa==it->second)//<- catch root
	break;
      taxa=it->second;
    }
    fprintf(fp,"\n");
}

int isuniq(std::vector<int> &vec){
  int ret = 1;
  for(int i=1;i<vec.size();i++)
    if(vec[0]!=vec[i])
      return 0;
  return ret;
}

int get_species1(int taxa,int2int &parent, int2char &rank){
  //  fprintf(stderr,"\t-> taxa:%d\n",taxa);
  int2char::iterator it;
  while(1){
    it=rank.find(taxa);
    if(it==rank.end())
      return -1;
    char *val = it->second;
    if(val==NULL)
      fprintf(stderr,"taxa:%d\n",taxa);
    assert(val);
    if(!strcmp(val,"species"))
      return taxa;
    int next = parent[taxa];
    if(next==taxa)
      break;
    taxa=next;
  }
  return taxa;
  
}

int2int get_species(int2int &i2i,int2int &parent, int2char &rank,int2char &names,FILE *fp){
  int2int ret;
  for(int2int::iterator it=i2i.begin();it!=i2i.end();it++){
    //fprintf(stderr,"%d\t%d\n",it->first,it->second);
    int asdf= get_species1(it->second,parent,rank);
    if(asdf==-1){
      fprintf(fp,"\t-> Removing pair(%d,%d) accnumber:%s since doesnt exists in node list\n",it->first,it->second,names[it->second]);
      i2i.erase(it);
    }else
      ret[it->second] = asdf;

  }
  return ret;
}

char *make_seq(bam1_t *aln){
  int len = aln->core.l_qseq;
  char *qseq = new char[len+1];
  uint8_t *q = bam_get_seq(aln);
  for(int i=0; i< len ; i++)
    qseq[i] = seq_nt16_str[bam_seqi(q,i)];
  qseq[len]='\0';
  //  fprintf(stderr,"seq:%s\n",qseq);
  //exit(0);
  return qseq;
}

int printonce=1;

std::vector<int> purge(std::vector<int> &taxids,std::vector<int> &editdist){
  if(printonce==1)
    fprintf(stderr,"\t-> purging taxids oldsize:%lu\n",taxids.size());
  assert(taxids.size()==editdist.size());
  std::vector<int> tmpnewvec;
  int mylow = *std::min_element(editdist.begin(),editdist.end());
  for(int i=0;i<taxids.size();i++)
    if(editdist[i]<=mylow)
      tmpnewvec.push_back(taxids[i]);
  if(printonce--==1)
    fprintf(stderr,"\t-> purging taxids newsize:%lu this info is only printed once\n",tmpnewvec.size());
  return tmpnewvec;
}
typedef struct{
  bam1_t **ary;
  size_t l;
  size_t m;
}queue;

queue *init_queue(size_t maxsize){
  queue *ret = new queue;
  ret->l =0;
  ret->m =maxsize;
  ret->ary = new bam1_t*[ret->m];
  for(int i=0;i<ret->m;i++)
    ret->ary[i] = bam_init1();
  return ret;
}

//expand queue with 500 elements
void expand_queue(queue *ret){
  bam1_t **newary = new bam1_t*[ret->m+500];
  for(int i=0;i<ret->l;i++)
    newary[i] = ret->ary[i];
  for(int i=ret->l;i<ret->m+500;i++)
    newary[i] = bam_init1();
  delete [] ret->ary;
  ret->ary = newary;
  ret->m += 500;
}


void hts(FILE *fp,samFile *fp_in,int2int &i2i,int2int& parent,bam_hdr_t *hdr,int2char &rank, int2char &name_map,FILE *log,int minmapq,int discard,int editMin, int editMax, double scoreLow,double scoreHigh,int minlength,char *lca_rank,char *prefix,int norank2species,int howmany){
  fprintf(stderr,"[%s] \t-> editMin:%d editmMax:%d scoreLow:%f scoreHigh:%f minlength:%d discard: %d prefix: %s howmany: %d\n",__FUNCTION__,editMin,editMax,scoreLow,scoreHigh,minlength,discard,prefix,howmany);
  assert(fp_in!=NULL);
  damage *dmg = new damage(howmany,8,13);
  bam1_t *aln = bam_init1(); //initialize an alignment
  int comp ;

  char *last=NULL;
  char *seq =NULL;
  std::vector<int> taxids;
  std::vector<int> specs;
  std::vector<int> editdist;
  queue *myq = init_queue(500);

  int lca;
  int2int closest_species;
  int skip=0;
  int inc=0;
  while(sam_read1(fp_in,hdr,aln) >= 0) {
    if(bam_is_unmapped(aln) ){
      // fprintf(stderr,"skipping: %s unmapped \n",bam_get_qname(b));
      continue;
    }
    if(bam_is_failed(aln) ){
      //fprintf(stderr,"skipping: %s failed: flags=%d \n",bam_get_qname(b),b->core.flag);
      continue;
    }
    char *qname = bam_get_qname(aln);
    int chr = aln->core.tid ; //contig name (chromosome)
    //    fprintf(stderr,"%d %d\n",aln->core.qual,minmapq);
    if(aln->core.qual<minmapq){
      fprintf(stderr,"discarding due to low mapq");
      continue;
    }
    
    if(discard>0&&(aln->core.flag&discard)){
      fprintf(stderr,"Discard: %d coreflag:%d OR %d\n",discard,aln->core.flag,aln->core.flag&discard);
      fprintf(stderr,"Discarding due to core flag\n");
      continue;
    }
    if(last==NULL){
      last=strdup(qname);
      seq=make_seq(aln);
    }
    if(minlength!=-1&&(aln->core.l_qseq<minlength))
      continue;
    //change of ref
    if(strcmp(last,qname)!=0) {
      if(taxids.size()>0&&skip==0){
	//	fprintf(stderr,"length of taxids:%lu and other:%lu minedit:%d\n",taxids.size(),editdist.size(),*std::min_element(editdist.begin(),editdist.end()));
	
	int size=taxids.size();
	assert(size==myq->l);
	if(editMin==-1&&editMax==-1)
	  taxids = purge(taxids,editdist);

	lca=do_lca(taxids,parent);
	if(0&&lca!=-1){
	  print_rank(stderr,lca,rank);
	  exit(0);
	}

	if(lca!=-1){
	  fprintf(fp,"%s:%s:%lu:%d",last,seq,strlen(seq),size);//fprintf(stderr,"size:%d\n");
	  //	  fprintf(stderr,"adfsadfsafafdad: %d size\n",size);
	  print_chain(fp,lca,parent,rank,name_map);
	  int varisunique = isuniq(specs);
	  //fprintf(stderr,"varisunquieu:%d spec.size():%lu\n",varisunique,specs.size());
	  if(varisunique){
	    int2int::iterator it=specWeight.find(specs[0]);
	    //fprintf(stderr,"specs: %d specs.size:%lu wiehg.szei():%lu\n",specs[0],specs.size(),specWeight.size());
	    if(it==specWeight.end())
	      specWeight[specs[0]] = 1;//specs.size();
	    else
	      it->second = it->second +1;
	    
	    //fprintf(stderr,"specs.size:%lu wiehg.szei():%lu\n",specs.size(),specWeight.size());
	  }
	  if(correct_rank(lca_rank,lca,rank,norank2species)){
	    for(int i=0;i<myq->l;i++){
	      int2int::iterator it2k = i2i.find(myq->ary[i]->core.tid);
	      assert(it2k!=i2i.end());
	      dmg->damage_analysis(myq->ary[i],it2k->second);
	    }
	  }
	}
      }
      skip=0;
      specs.clear();
      editdist.clear();
      myq->l = 0;
      free(last);
      delete [] seq;
      last=strdup(qname);
      seq=make_seq(aln);
    }
    
    
    //filter by nm
    uint8_t *nm = bam_aux_get(aln,"NM");
    int thiseditdist;
    if(nm!=NULL){
      thiseditdist = (int) bam_aux2i(nm);
      //      fprintf(stderr,"[%d] nm:%d\t",inc++,val);
      if(editMin!=-1&&thiseditdist<editMin){
	skip=1;
	//fprintf(stderr,"skipped1\n");
	continue;
      }else if(editMax!=-1&&thiseditdist>editMax){
	//fprintf(stderr,"continued1\n");
	continue;
      }
      double seqlen=aln->core.l_qseq;
      double myscore=1.0-(((double) thiseditdist)/seqlen);
      //      fprintf(stderr," score:%f\t",myscore);
      if(myscore>scoreHigh){
	//fprintf(stderr,"skipped2\n");
	skip=1;
	continue;
      }
      else if(myscore<scoreLow){	
	//	fprintf(stderr,"continued2\n");
	continue;
      }
    }
    int2int::iterator it = i2i.find(chr);
    //See if cloests speciest exists and plug into closests species
    int dingdong=-1;
    if(it!=i2i.end()) {
      int2int::iterator it2=closest_species.find(chr);
      if(it2!=closest_species.end())
	dingdong=it->second;
      else{
	dingdong=get_species1(it->second,parent,rank);
	if(dingdong!=-1)
	  closest_species[it->second]=dingdong;
      }
      //fprintf(stderr,"\t-> closests size:%lu\n",closest_species.size());
    }

    if(it==i2i.end()||dingdong==-1){
      int2int::iterator it2missing = i2i_missing.find(chr);
      if(it2missing==i2i_missing.end()){
	fprintf(log,"\t-> problem finding chrid:%d chrname:%s\n",chr,hdr->target_name[chr]);
	i2i_missing[chr] = 1;
      }else
	it2missing->second = it2missing->second + 1;
    }else{
      assert(bam_copy1(myq->ary[myq->l],aln)!=NULL);
      myq->l++;
      if(myq->l==myq->m)
	expand_queue(myq);
      taxids.push_back(it->second);
      specs.push_back(dingdong);
      editdist.push_back(thiseditdist);
      
      //fprintf(stderr,"it-.second:%d specs:%d thiseditdist:%d\n",it->second,dingdong,thiseditdist);
      //      fprintf(stderr,"EDIT\t%d\n",thiseditdist);
    }
  }
  if(taxids.size()>0&&skip==0){
    int size=taxids.size();
    assert(size==myq->l);
    if(editMin==-1&&editMax==-1)
      taxids = purge(taxids,editdist);
    
    lca=do_lca(taxids,parent);
    if(lca!=-1){
      fprintf(fp,"%s:%s:%lu:%d",last,seq,strlen(seq),size);
      print_chain(fp,lca,parent,rank,name_map);
      if(isuniq(specs)){
	int2int::iterator it=specWeight.find(specs[0]);
	if(it==specWeight.end())
	  specWeight[specs[0]] = specs.size();
	else
	  it->second = it->second +specs.size();
	
      }
      if(correct_rank(lca_rank,lca,rank,norank2species)){
	for(int i=0;i<myq->l;i++){
	  //dmg->damage_analysis(myq->ary[i],myq->ary[i]->core.tid);
	  int2int::iterator ittt = i2i.find(myq->ary[i]->core.tid);
	  assert(ittt!=i2i.end());
	  dmg->damage_analysis(myq->ary[i],ittt->second);
	}
      }
    }
  }
  dmg->bwrite(prefix,hdr);
  myq->l = 0;
  specs.clear();
  editdist.clear();
  bam_destroy1(aln);
  sam_close(fp_in);
  
  
  
  return ;//0;
}

void print_ref_rank_species(bam_hdr_t *h,int2int &i2i,int2char &names,int2char &rank){
  for(int i=0;i<h->n_targets;i++){
    fprintf(stdout,"%d\t%s\t%s\n",i,names[i2i[i]],rank[i2i[i]]);
    
  }

}

int calc_valens(int2int &i2i, int2int &parent){
  int2int ret;
  for(int2int::iterator it=i2i.begin();it!=i2i.end();it++){
    int2int::iterator pit=parent.find(it->second);
    if(pit==parent.end()||pit->second==1)
      continue;
    //    fprintf(stdout,"%d %d\n",pit->first,pit->second);
    int2int::iterator rit=ret.find(pit->second);
    if(rit==ret.end())
      ret[pit->second]=1;
    else
      rit->second = rit->second +1;
  }

  for(int2int::iterator it=ret.begin();it!=ret.end();it++)
    fprintf(stdout,"%d\t%d\n",it->first,it->second);
  return 0;
}



int calc_dist2root(int2int &i2i, int2int &parent){
  int2int ret;
  for(int2int::iterator it=i2i.begin();it!=i2i.end();it++){
    int2int::iterator pit=parent.find(it->second);
    if(pit==parent.end()||pit->second==1)
      continue;
    //    fprintf(stdout,"%d %d\n",pit->first,pit->second);
    int2int::iterator rit=ret.find(pit->second);
    if(rit==ret.end())
      ret[pit->second]=1;
    else
      rit->second = rit->second +1;
  }

  for(int2int::iterator it=ret.begin();it!=ret.end();it++)
    fprintf(stdout,"%d\t%d\n",it->first,it->second);
  return 0;
}
#if 0
int2node makeNodes(int2int &parent){
  int2node ret;
  for(int2int::iterator it=parent.begin();it!=parent.end();it++){
    int2node::iterator it2=ret.find(it->second);
    if(it2==ret.end()){
      node nd;
      nd.up=it->second;
      ret[it->first] = nd;
    }
    
  }
  return ret;
}
#endif

int main_lca(int argc, char **argv){
  if(argc==1){
    fprintf(stderr,"\t-> ./ngsLCA -names -nodes -acc2tax [-editdist[min/max] -simscore[low/high] -minmapq -discard] -bam -lca_rank version: %s\n",METADAMAGE_VERSION);
    return 0;
  }
  catchkill();
#if 0
  int2int ww = get_weight(argv[1]);
  return 0;
#endif
  time_t t2=time(NULL);

  pars *p=get_pars(--argc,++argv);
  print_pars(stderr,p);
  fprintf(p->fp1,"# ./ngsLCA ");
  for(int i=0;i<argc;i++)
    fprintf(p->fp1," %s",argv[i]);
  fprintf(p->fp1," version: %s\n",METADAMAGE_VERSION);
  //map of bamref ->taxid

  int2int *i2i=NULL;
fprintf(stderr,"p->header: %p\n",p->header);
  if(p->header)
    i2i=(int2int*) bamRefId2tax(p->header,p->acc2taxfile,p->htsfile);

  //map of taxid -> taxid
  int2int parent;
  //map of taxid -> rank
  int2char rank;
  //map of taxid -> name
  //map of parent -> child taxids
  int2intvec child;
  
  int2char name_map = parse_names(p->namesfile);
  
  parse_nodes(p->nodesfile,rank,parent,child,0);

  //  calc_valens(i2i,parent);
  if(0){
    print_ref_rank_species(p->header,*i2i,name_map,rank);
    return 0;
  }

  //closes species (direction of root) for a taxid
  //  int2int closest_species=get_species(i2i,parent,rank,name_map,p->fp3);
  //  fprintf(stderr,"\t-> Number of items in closest_species map:%lu\n",closest_species.size());

  
  fprintf(stderr,"\t-> Will add some fixes of the ncbi database due to merged names\n");
  mod_db(mod_in,mod_out,parent,rank,name_map);

  hts(p->fp1,p->hts,*i2i,parent,p->header,rank,name_map,p->fp3,p->minmapq,p->discard,p->editdistMin,p->editdistMax,p->simscoreLow,p->simscoreHigh,p->minlength,p->lca_rank,p->outnames,p->norank2species,p->howmany);

  fprintf(stderr,"\t-> Number of species with reads that map uniquely: %lu\n",specWeight.size());
  
  for(int2int::iterator it=errmap.begin();it!=errmap.end();it++)
    fprintf(p->fp3,"err\t%d\t%d\n",it->first,it->second);
#if 0
  for(int2int::iterator it=i2i_missing.begin();it!=i2i_missing.end();it++)
    fprintf(p->fp3,"missingtaxid \t%d\t%d\t%s\n",it->first,it->second,p->header[it->first]);
#endif
  //p->header points to bam_hdr_t what is expected here?
  for(int2int::iterator it=specWeight.begin();0&&it!=specWeight.end();it++)
    fprintf(p->fp2,"%d\t%s\t%d\n",it->first,name_map[it->first],it->second);
  pars_free(p);
  fprintf(stderr, "\t-> [ALL done] walltime used =  %.2f sec\n", (float)(time(NULL) - t2));  
  return 0;
}