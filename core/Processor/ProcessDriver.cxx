#ifndef PROCESSDRIVER_CXX
#define PROCESSDRIVER_CXX
#include <sstream>
#include "ProcessDriver.h"
#include "ProcessFactory.h"
#include "Base/UtilFunc.h"
namespace larcv {

  ProcessDriver::ProcessDriver(std::string name)
    : larcv_base(name)
    , _enable_filter(false)
    , _random_access(false)
    , _proc_v()
    , _processing(false)
    , _fout(nullptr)
    , _fout_name("")
  {}

  void ProcessDriver::reset()
  {
    LARCV_DEBUG() << "Called" << std::endl;
    _io.reset();
    _enable_filter = _random_access = false;
    for(size_t i=0; i<_proc_v.size(); ++i) { delete _proc_v[i]; _proc_v[i]=nullptr; }
    _proc_v.clear();
    _processing = false;
    _fout = nullptr;
    _fout_name = "";
  }

  void ProcessDriver::override_input_file(const std::vector<std::string>& flist)
  {
    LARCV_DEBUG() << "Called" << std::endl;
    _io.clear_in_file();
    for(auto const& f : flist) _io.add_in_file(f);
  }

  ProcessID_t ProcessDriver::process_id(std::string name) const
  {
    LARCV_DEBUG() << "Called" << std::endl;
    auto iter = _proc_m.find(name);
    if(iter == _proc_m.end()) {
      LARCV_CRITICAL() << "Process w/ name " << name << " not found..." << std::endl;
      throw larbys();
    }
    return (*iter).second;
  }

  std::vector<std::string> ProcessDriver::process_names() const
  {
    LARCV_DEBUG() << "Called" << std::endl;
    std::vector<std::string> res;
    res.reserve(_proc_m.size());
    for(auto const& name_id : _proc_m) res.push_back(name_id.first);
    return res;
  }

  const std::map<std::string,larcv::ProcessID_t>& ProcessDriver::process_map() const
  { LARCV_DEBUG() << "Called" << std::endl; return _proc_m; }

  const ProcessBase* ProcessDriver::process_ptr(size_t id) const
  {
    LARCV_DEBUG() << "Called" << std::endl;
    if(id >= _proc_v.size()) {
      LARCV_CRITICAL() << "Invalid ID requested: " << id << std::endl;
      throw larbys();
    }
    return _proc_v[id];
  }

  void ProcessDriver::configure(const std::string config_file)
  {
    LARCV_DEBUG() << "Called" << std::endl;
    // check state
    if(_processing) {
      LARCV_CRITICAL() << "Must call finalize() before calling initialize() after starting to process..." << std::endl;
      throw larbys();
    }
    // check cfg file
    if(config_file.empty()) {
      LARCV_CRITICAL() << "Config file not set!" << std::endl;
      throw larbys();
    }

    // check cfg content top level
    auto main_cfg = CreatePSetFromFile(config_file);
    if(!main_cfg.contains_pset(name())) {
      LARCV_CRITICAL() << "ProcessDriver configuration (" << name() << ") not found in the config file (dump below)" << std::endl
		       << main_cfg.dump()
		       << std::endl;
      throw larbys();
    }
    auto const cfg = main_cfg.get<larcv::PSet>(name());
    // check io config exists
    if(!cfg.contains_pset("IOManager")) {
      LARCV_CRITICAL() << "IOManager config not found!" << std::endl
		       << cfg.dump()
		       << std::endl;
      throw larbys();
    }
    // check process config exists
    if(!cfg.contains_pset("ProcessList")) {
      LARCV_CRITICAL() << "ProcessList config not found!" << std::endl
		       << cfg.dump()
		       << std::endl;
      throw larbys();
    }

    reset();
    LARCV_INFO() << "Retrieving IO config" << std::endl;
    auto const io_config = cfg.get<larcv::PSet>("IOManager");
    LARCV_INFO() << "Retrieving ProcessList" << std::endl;
    auto const proc_config = cfg.get<larcv::PSet>("ProcessList");

    // Prepare IO manager
    LARCV_INFO() << "Configuring IO" << std::endl;
    _io = IOManager(io_config);

    // Set ProcessDriver
    LARCV_INFO() << "Retrieving self (ProcessDriver) config" << std::endl;
    set_verbosity((msg::Level_t)(cfg.get<unsigned short>("Verbosity",logger().level())));
    _enable_filter = cfg.get<bool>("EnableFilter");
    _random_access = cfg.get<bool>("RandomAccess");
    _fout_name = cfg.get<std::string>("AnaFile","");
    
    // Prepare Process list: loop over fcl table and use key as an instance name
    LARCV_INFO() << "Start looping process list to instantiate processes" << std::endl;
    for(auto const& proc_name : proc_config.pset_keys()) {

      // Retrieve process configuration
      auto const& proc_cfg  = proc_config.get_pset(proc_name);
      std::string proc_type = "";
      // Attempt to retrieve process class name
      try{
	proc_type = proc_cfg.get<std::string>("ProcessType");
      }catch(const larbys& err){
	LARCV_CRITICAL() << "Could not locate ProcessType key..." << std::endl
			 << proc_cfg.dump()
			 << std::endl;
	throw larbys();
      }
      LARCV_INFO() << "Instantiating Process type: \033[93m" << proc_type << "\033[00m w/ a name \033[93m" << proc_name << "\033[00m" << std::endl;
      // Attempt to instantiate retrieved class + register its ID
      auto ptr = ProcessFactory::get().create(proc_type,proc_name);
      ptr->_id = _proc_v.size();
      _proc_m.emplace(ptr->name(),_proc_v.size()-1);

      // Configure process
      LARCV_INFO() << "Assigned ProcessID_t: " << ptr->_id << " configuring the instance..." << std::endl;
      ptr->configure(proc_cfg);
      // Register instance pointer
      _proc_v.push_back(ptr);
    }
  }

  void ProcessDriver::initialize()
  {
    LARCV_DEBUG() << "Called" << std::endl;
    // check state
    if(_processing) {
      LARCV_CRITICAL() << "Must call finalize() before calling initialize() after starting to process..." << std::endl;
      throw larbys();
    }

    // Initialize process
    for(auto& p : _proc_v) {
      LARCV_INFO() << "Initializing: " << p->name() << std::endl;
      p->initialize();
    }

    // Initialize IO
    LARCV_INFO() << "Initializing IO " << std::endl;
    _io.initialize();

    // Handle invalid cases
    auto const nentries = _io.get_n_entries();
    auto const io_mode  = _io.io_mode();
    
    // Random access + write mode cannot be combined
    if(_random_access && io_mode == IOManager::kWRITE) {
      LARCV_CRITICAL() << "Random access mode not supported for kWRITE IO mode!" << std::endl;
      throw larbys();
    }
    // No entries means nothing to do unless write mode
    if(!nentries && io_mode != IOManager::kWRITE) {
      LARCV_CRITICAL() << "No entries found from IO (kREAD/kBOTH mode cannot be run)!" << std::endl;
      throw larbys();
    }

    // Prepare analysis output file if needed
    LARCV_INFO() << "Opening analysis output file" << std::endl;
    if(!_fout_name.empty()) _fout = TFile::Open(_fout_name.c_str(),"RECREATE");

    // Change state from to-be-initialized to to-process
    _processing = true;

    // Prepare ttree entry index array to follow in execution (randomize if specified
    LARCV_INFO() << "Preparing access index vector" << std::endl;
    if(nentries) {
      _access_entry_v.resize(nentries);
      for(size_t i=0; i<_access_entry_v.size(); ++i) _access_entry_v[i] = i;
      if(_random_access) std::random_shuffle(_access_entry_v.begin(),_access_entry_v.end());
    }

    _current_entry = 0;
  }

  bool ProcessDriver::_process_entry_()
  {
    // Private method to execute processes and change entry number record
    // This method does not perform any sanity check, hence private and 
    // should be used by wrapper method which performs necessary checks.

    // Execute
    bool good_status=true;
    for(auto& p : _proc_v) {
      good_status = good_status && p->process(_io);
      if(!good_status && _enable_filter) break;
    }
    // If not read mode save entry
    if(_io.io_mode() != IOManager::kREAD && (!_enable_filter || good_status)) _io.save_entry();    
    // Bump up entry record
    ++_current_entry;

    return good_status;
  }

  bool ProcessDriver::process_entry()
  {
    LARCV_DEBUG() << "Called" << std::endl;
    // Public method to process "next" entry

    // Check state
    if(!_processing) {
      LARCV_CRITICAL() << "Must call initialize() before start processing!" << std::endl;
      throw larbys();
    }

    // Check if input entry exists in case of read/both io mode
    if(_io.io_mode() != IOManager::kWRITE) {

      if(_access_entry_v.size() <= _current_entry) {
	LARCV_NORMAL() << "Entry " << _current_entry << " exceeds available events in a file!" << std::endl;
	return false;
      }
      // if exist then move read pointer
      _io.read_entry(_access_entry_v[_current_entry]);
    }
    // Execute processes
    return _process_entry_();
  }
  
  bool ProcessDriver::process_entry(size_t entry)
  {
    LARCV_DEBUG() << "Called" << std::endl;
    // Public method to process "specified" entry

    // Check state
    if(!_processing) {
      LARCV_CRITICAL() << "Must call initialize() before start processing!" << std::endl;
      throw larbys();
    }

    // Check if input entry exists in case of read/both io mode
    if(_io.io_mode() != IOManager::kWRITE) {

      if(_access_entry_v.size() <= entry) {
	LARCV_ERROR() << "Entry " << entry << " exceeds available events in a file!" << std::endl;
	return false;
      }
      // if exist then move read pointer      
      _io.read_entry(_access_entry_v[entry]);
      _current_entry = entry;
    }
    // Execute processes
    return _process_entry_();
  }
  
  void ProcessDriver::batch_process(size_t start_entry,size_t num_entries){
    LARCV_DEBUG() << "Called" << std::endl;
    // Public method to execute num_entries starting from start_entry

    // Check state
    if(!_processing) {
      LARCV_CRITICAL() << "Must call initialize() before start processing!" << std::endl;
      throw larbys();
    }

    // Define the max entry index for processing (1 above last entry to be processed)
    size_t max_entry = start_entry + num_entries;

    // Check if start_entry is 0 for write mode (no entry should be specified for write mode)
    if(_io.io_mode() == IOManager::kWRITE){
      if(start_entry) {
	LARCV_CRITICAL() << "Cannot specify start entry (1st arg) in kWRITE IO mode!" << std::endl;
	throw larbys();
      }
      max_entry = _current_entry + num_entries;

    }else{
      _current_entry = start_entry;
      if(!num_entries) max_entry = start_entry + _io.get_n_entries();
    }

    // Make sure max entry does not exceed the physical max from input. If so, truncate.
    if(_io.io_mode() != IOManager::kWRITE && max_entry > _access_entry_v.size()){
      LARCV_WARNING() << "Requested to process entries from " << start_entry << " to " << max_entry-1 
		      << " ... but there are only " << _access_entry_v.size() << " entries in input!" << std::endl
		      << "Truncating the end entry to " << _access_entry_v.size()-1 << std::endl;
      max_entry = _access_entry_v.size();
    }

    // Batch-execute in while loop
    size_t num_processed=0;
    size_t num_fraction=(max_entry - _current_entry)/10;
    while(_current_entry < max_entry) {
      
      if(_io.io_mode() != IOManager::kWRITE) 
	
	_io.read_entry(_access_entry_v[_current_entry]);

      _process_entry_();

      ++num_processed;
      if(!num_fraction) { 
	LARCV_NORMAL() << "Processed " << num_processed << " entries..." << std::endl;
      }else if(num_processed%num_fraction==0) {
	LARCV_NORMAL() << "Processed " << 10*int(num_processed/num_fraction) << " %..." << std::endl;
      }
    }

  }
  
  void ProcessDriver::finalize()
  {
    LARCV_DEBUG() << "called" << std::endl;

    for(auto& p : _proc_v) {
      LARCV_INFO() << "Finalizing: " << p->name() << std::endl;
      p->finalize(_fout);
    }

    // Profile repor
    LARCV_INFO() << "Compiling time profile..." << std::endl;
    std::stringstream ss;
    for(auto& p : _proc_v) {
      if(!p->_profile) continue;
      ss << "  \033[93m" << std::setw(20) << std::setfill(' ') << p->name() << "\033[00m"
	 << " ... # call " << std::setw(5) << std::setfill(' ') << p->_proc_count
	 << " ... total time " << p->_proc_time << " [s]"
	 << " ... average time " << p->_proc_time / p->_proc_count << " [s/process]"
	 << std::endl;
    }
    std::string msg(ss.str());
    if(!msg.empty()) 
      LARCV_NORMAL() << "Simple time profiling requested and run..."
		     << "  ================== " << name() << " Profile Report ==================" << std::endl
		     << msg
		     << std::endl;

    LARCV_INFO() << "Closing analysis output file..." << std::endl;
    if(_fout) _fout->Close();
    LARCV_INFO() << "Finalizing IO..." << std::endl;
    _io.finalize();
    LARCV_INFO() << "Resetting..." << std::endl;
    reset();
  }

}

#endif