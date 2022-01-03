#ifndef EPIWORLD_MODEL_MEAT_HPP
#define EPIWORLD_MODEL_MEAT_HPP

#define CHECK_INIT() if (!initialized) \
        throw std::logic_error("Model not initialized.");

#define NEXT_STATUS() \
    /* Making the change effective */ \
    for (auto & p: persons) \
        if (!IN(p.status, status_removed)) \
            p.status = p.status_next;

template<typename TSeq>
inline Model<TSeq>::Model(const Model<TSeq> & model) :
    db(model.db),
    persons(model.persons),
    persons_ids(model.persons_ids),
    viruses(model.viruses),
    prevalence_virus(model.prevalence_virus),
    tools(model.tools),
    prevalence_tool(model.prevalence_tool),
    engine(model.engine),
    runifd(model.runifd),
    parameters(model.parameters),
    ndays(model.ndays),
    pb(model.pb),
    verbose(model.verbose),
    initialized(model.initialized),
    current_date(model.current_date)
{

    // Pointing to the right place
    db.set_model(*this);

    // Removing old neighbors
    for (auto & p: persons)
        p.neighbors.clear();
    
    // Rechecking individuals
    for (unsigned int p = 0u; p < size(); ++p)
    {
        // Making room
        const Person<TSeq> & person_this = model.persons[p];
        Person<TSeq> & person_res  = persons[p];

        // Person pointing to the right model and person
        person_res.model        = this;
        person_res.viruses.host = &person_res;
        person_res.tools.person = &person_res;

        // Readding
        std::vector< Person<TSeq> * > neigh = person_this.neighbors;
        for (unsigned int n = 0u; n < neigh.size(); ++n)
        {
            // Point to the right neighbors
            int loc = persons_ids[neigh[n]->get_id()];
            person_res.add_neighbor(&persons[loc], true, true);

        }

    }

    // Finally, seeds are resetted automatically based on the original
    // engine
    seed(floor(model.runifd() * UINT_MAX));

}

template<typename TSeq>
inline DataBase<TSeq> & Model<TSeq>::get_db()
{
    return db;
}

template<typename TSeq>
inline std::vector<Person<TSeq>> * Model<TSeq>::get_persons()
{
    return &persons;
}

template<typename TSeq>
inline void Model<TSeq>::set_rand_engine(std::mt19937 & eng)
{
    engine = std::make_shared< std::mt19937 >(eng);
}

template<typename TSeq>
inline double & Model<TSeq>::operator()(std::string pname) {

    if (parameters.find(pname) == parameters.end())
        throw std::range_error("The parameter "+ pname + "is not in the model.");

    return parameters[pname];

}

template<typename TSeq>
inline size_t Model<TSeq>::size() const {
    return persons.size();
}

template<typename TSeq>
inline void Model<TSeq>::init(
    unsigned int ndays,
    unsigned int seed
    ) {

    EPIWORLD_CLOCK_START("(00) Init model")

    if (initialized) 
        throw std::logic_error("Model already initialized.");

    // Setting up the number of steps
    this->ndays = ndays;

    // Initializing persons
    for (auto & p : persons)
    {
        p.model = this;
        p.init();
    }

    engine->seed(seed);

    // if (!runifd)
    //     runifd = std::make_shared< std::uniform_real_distribution<> >(0.0, 1.0);

    initialized = true;

    // Starting first infection and tools
    reset();

    EPIWORLD_CLOCK_END("(00) Init model")

}

template<typename TSeq>
inline void Model<TSeq>::dist_virus()
{

    // Starting first infection
    for (unsigned int v = 0; v < viruses.size(); ++v)
    {
        for (auto & p : persons)
            if (runif() < prevalence_virus[v])
            {
                p.add_virus(0, viruses[v]);
                db.up_infected(&viruses[v], p.get_status(), STATUS::INFECTED);
            }

    }

    NEXT_STATUS()

}

template<typename TSeq>
inline void Model<TSeq>::dist_tools()
{
    // Tools
    for (unsigned int t = 0; t < tools.size(); ++t)
    {
        for (auto & p : persons)
            if (runif() < prevalence_tool[t])
                p.add_tool(0, tools[t]);

    }

}

template<typename TSeq>
inline std::mt19937 * Model<TSeq>::get_rand_endgine()
{
    return engine.get();
}

template<typename TSeq>
inline double Model<TSeq>::runif() {
    // CHECK_INIT()
    return runifd->operator()(*engine);
}

template<typename TSeq>
inline void Model<TSeq>::seed(unsigned int s) {
    this->engine->seed(s);
}

template<typename TSeq>
inline void Model<TSeq>::add_virus(Virus<TSeq> v, double preval)
{

    // Setting the id
    v.set_id(viruses.size());
    
    // Adding new virus
    viruses.push_back(v);
    prevalence_virus.push_back(preval);


}

template<typename TSeq>
inline void Model<TSeq>::add_tool(Tool<TSeq> t, double preval)
{
    tools.push_back(t);
    prevalence_tool.push_back(preval);
}

template<typename TSeq>
inline void Model<TSeq>::pop_from_adjlist(
    std::string fn,
    int skip,
    bool directed,
    int min_id,
    int max_id
    ) {

    AdjList al;
    al.read_edgelist(fn, skip, directed);
    this->pop_from_adjlist(al);

}

template<typename TSeq>
inline void Model<TSeq>::pop_from_adjlist(AdjList al) {

    // Resizing the people
    persons.clear();
    persons_ids.clear();
    persons.resize(al.vcount(), Person<TSeq>());

    const auto & tmpdat = al.get_dat();
    
    int loc;
    for (const auto & n : tmpdat)
    {
        if (persons_ids.find(n.first) == persons_ids.end())
            persons_ids[n.first] = persons_ids.size();

        loc = persons_ids[n.first];

        persons[loc].model = this;
        persons[loc].id    = n.first;
        persons[loc].index = loc;

        for (const auto & link: n.second)
        {
            if (persons_ids.find(link.first) == persons_ids.end())
                persons_ids[link.first] = persons_ids.size();

            unsigned int loc_link   = persons_ids[link.first];
            persons[loc_link].id    = link.first;
            persons[loc_link].index = loc_link;

            persons[loc].add_neighbor(
                &persons[persons_ids[link.first]],
                true, true
                );
        }

    }

}

template<typename TSeq>
inline int Model<TSeq>::today() const {
    return this->current_date;
}

template<typename TSeq>
inline void Model<TSeq>::next() {

    db.record();
    ++this->current_date;
    
    // Advicing the progress bar
    if (verbose)
        pb.next();

    return ;
}

template<typename TSeq>
inline void Model<TSeq>::update_status() {

    // Next status
    for (auto & p: persons)
        p.update_status();

    NEXT_STATUS()

}

template<typename TSeq>
inline void Model<TSeq>::mutate_variant() {

    for (auto & p: persons)
    {
        if (IN(p.get_status(), status_infected))
            p.mutate_variant();

    }

}

template<typename TSeq>
inline void Model<TSeq>::record_variant(Virus<TSeq> * v) {

    // Updating registry
    db.record_variant(v);
    return;
    
} 

template<typename TSeq>
inline int Model<TSeq>::get_nvariants() const {
    return db.size();
}

template<typename TSeq>
inline const std::vector<TSeq> & Model<TSeq>::get_variant_sequence() const {
    return db.get_sequence();
}

template<typename TSeq>
inline const std::vector<int> & Model<TSeq>::get_variant_nifected() const {
    return db.get_today_variant("ninfected");
}

template<typename TSeq>
inline unsigned int Model<TSeq>::get_ndays() const {
    return ndays;
}

template<typename TSeq>
inline void Model<TSeq>::set_ndays(unsigned int ndays) {
    this->ndays = ndays;
}

template<typename TSeq>
inline bool Model<TSeq>::get_verbose() const {
    return verbose;
}

template<typename TSeq>
inline void Model<TSeq>::verbose_on() {
    verbose = true;
}

template<typename TSeq>
inline void Model<TSeq>::verbose_off() {
    verbose = false;
}

template<typename TSeq>
inline void Model<TSeq>::rewire_degseq(double proportion)
{

    // Identifying individuals with degree > 0
    std::vector< unsigned int > non_isolates;
    std::vector< double > weights;
    double nedges = 0.0;
    for (unsigned int i = 0u; i < persons.size(); ++i)
    {
        if (persons[i].neighbors.size() > 0u)
        {
            non_isolates.push_back(i);
            weights.push_back(
                static_cast<double>(persons[i].neighbors.size())
                );
            nedges += 1.0;
        }
    }

    if (non_isolates.size() == 0u)
        throw std::logic_error("The graph is completely disconnected.");

    // Cumulative probs
    weights[0u] /= nedges;
    for (unsigned int i = 1u; i < non_isolates.size(); ++i)
    {
         weights[i] /= nedges;
         weights[i] += weights[i - 1u];
    }

    // Only swap if needed
    unsigned int N = non_isolates.size();
    double prob;
    int nrewires = floor(proportion * nedges);
    while (nrewires-- > 0)
    {

        // Picking egos
        prob = runif();
        int id0 = N - 1;
        for (unsigned int i = 0u; i < N; ++i)
            if (prob <= weights[i])
            {
                id0 = i;
                break;
            }

        prob = runif();
        int id1 = N - 1;
        for (unsigned int i = 0u; i < N; ++i)
            if (prob <= weights[i])
            {
                id1 = i;
                break;
            }

        // Correcting for under or overflow.
        if (id1 == id0)
            id1++;

        if (id1 >= static_cast<int>(N))
            id1 = 0;

        Person<TSeq> & p0 = persons[non_isolates[id0]];
        Person<TSeq> & p1 = persons[non_isolates[id1]];

        // Picking alters (relative location in their lists)
        // In this case, these are uniformly distributed within the list
        int id01 = std::floor(p0.neighbors.size() * runif());
        int id11 = std::floor(p1.neighbors.size() * runif());

        // When rewiring, we need to flip the individuals from the other
        // end as well, since we are dealing withi an undirected graph

        // Finding what neighbour is id0
        unsigned int n0,n1;
        Person<TSeq> & p01 = persons[p0.neighbors[id01]->index];
        for (n0 = 0; n0 < p01.neighbors.size(); ++n0)
        {
            if (p0.id == p01.neighbors[n0]->get_id())
                break;            
        }

        Person<TSeq> & p11 = persons[p1.neighbors[id11]->index];
        for (n1 = 0; n1 < p11.neighbors.size(); ++n1)
        {
            if (p1.id == p11.neighbors[n1]->get_id())
                break;            
        }

        // Moving alter first
        std::swap(p0.neighbors[id01], p1.neighbors[id11]);
        std::swap(p01.neighbors[n0], p11.neighbors[n1]);
        
    }

    return;

}

template<typename TSeq>
inline void Model<TSeq>::write_data(
    std::string fn_variant_info,
    std::string fn_variant_hist,
    std::string fn_total_hist,
    std::string fn_transmission
    ) const
{

    db.write_data(fn_variant_info,fn_variant_hist,fn_total_hist,fn_transmission);

}

template<typename TSeq>
inline void Model<TSeq>::write_edgelist(
    std::string fn
    ) const
{

    EPIWORLD_CLOCK_START("(03) Writing edgelist")

    std::ofstream efile(fn, std::ios_base::out);
    efile << "source target\n";
    for (const auto & p : persons)
    {
        for (auto & n : p.neighbors)
            efile << p.id << " " << n->id << "\n";
    }

    EPIWORLD_CLOCK_END("(03) Writing edgelist")

}

template<typename TSeq>
inline std::map<std::string,double> & Model<TSeq>::params()
{
    return parameters;
}

template<typename TSeq>
inline void Model<TSeq>::reset() {
    
    // Restablishing people
    pb = Progress(ndays, 80);

    for (auto & p : persons)
        p.reset();
    
    current_date = 0;

    db.set_model(*this);

    // Recording variants
    for (Virus<TSeq> & v : viruses)
        record_variant(&v);

    // Re distributing tools and virus
    dist_virus();
    dist_tools();

    

}

template<typename TSeq>
inline void Model<TSeq>::print() const
{
    printf_epiworld("Population size   : %i\n", static_cast<int>(size()));
    printf_epiworld("Days (duration)   : %i (of %i)\n", today(), ndays);
    printf_epiworld("Number of variants: %i\n\n", static_cast<int>(db.get_nvariants()));
    printf_epiworld("Virus(es):\n");
    int i = 0;
    for (auto & v : viruses)
    {    printf_epiworld(
            " - %s (baseline prevalence: %.2f)\n",
            v.get_name().c_str(),
            prevalence_virus[i++]
            );
    }

    printf_epiworld("Tool(s):\n");
    i = 0;
    for (auto & t : tools)
    {    printf_epiworld(
            " - %s (baseline prevalence: %.2f)\n",
            t.get_name().c_str(),
            prevalence_tool[i++]
            );
    }

    // Information about the parameters included
    printf_epiworld("\nModel parameters:\n");
    unsigned int nchar = 0u;
    for (auto & p : parameters)
        if (p.first.length() > nchar)
            nchar = p.first.length();

    std::string fmt = " - %-" + std::to_string(nchar + 1) + "s: ";
    for (auto & p : parameters)
    {
        std::string fmt_tmp = fmt;
        if (std::fabs(p.second) < 0.0001)
            fmt_tmp += "%.1e\n";
        else
            fmt_tmp += "%.4f\n";

        printf_epiworld(
            fmt_tmp.c_str(),
            p.first.c_str(),
            p.second
        );

        
    }


    nchar = 0u;
    for (auto & p : status_susceptible_labels)
        if (p.length() > nchar)
            nchar = p.length();
    
    for (auto & p : status_infected_labels)
        if (p.length() > nchar)
            nchar = p.length();

    for (auto & p : status_removed_labels)
        if (p.length() > nchar)
            nchar = p.length();

    if (initialized) 
        fmt = " - Total %-" + std::to_string(nchar + 1) + "s: %i\n";
    else
        fmt = " - Total %-" + std::to_string(nchar + 1) + "s: %s\n";
        
    printf_epiworld("\nStatistics (susceptible):\n");
    for (unsigned int s = 0u; s < status_susceptible.size(); ++s)
    {
        if (initialized)
        {
            
            printf_epiworld(
                fmt.c_str(),
                status_susceptible_labels[s].c_str(),
                db.today_total[ status_susceptible[s] ]
                );

        } else {
            printf_epiworld(
                fmt.c_str(),
                status_susceptible_labels[s].c_str(),
                " - "
                );
        }
    }

    printf_epiworld("\nStatistics (infected):\n");
    for (unsigned int s = 0u; s < status_infected.size(); ++s)
    {
        if (initialized)
        {
            
            printf_epiworld(
                fmt.c_str(),
                status_infected_labels[s].c_str(),
                db.today_total[ status_infected[s] ]
                );

        } else {
            printf_epiworld(
                fmt.c_str(),
                status_infected_labels[s].c_str(),
                " - "
                );
        }
    }

    printf_epiworld("\nStatistics (removed):\n");
    for (unsigned int s = 0u; s < status_removed.size(); ++s)
    {
        if (initialized)
        {
            
            printf_epiworld(
                fmt.c_str(),
                status_removed_labels[s].c_str(),
                db.today_total[ status_removed[s] ]
                );

        } else {
            printf_epiworld(
                fmt.c_str(),
                status_removed_labels[s].c_str(),
                " - "
                );
        }
    }
    

    return;

}

template<typename TSeq>
inline Model<TSeq> && Model<TSeq>::clone() const {

    // Step 1: Regen the individuals and make sure that:
    //  - Neighbors point to the right place
    //  - DB is pointing to the right place
    Model<TSeq> res(*this);

    // Pointing to the right place
    res.get_db().set_model(res);

    // Removing old neighbors
    for (auto & p: res.persons)
        p.neighbors.clear();
    
    // Rechecking individuals
    for (unsigned int p = 0u; p < size(); ++p)
    {
        // Making room
        const Person<TSeq> & person_this = persons[p];
        Person<TSeq> & person_res  = res.persons[p];

        // Person pointing to the right model and person
        person_res.model        = &res;
        person_res.viruses.host = &person_res;
        person_res.tools.person = &person_res;

        // Readding
        std::vector< Person<TSeq> * > neigh = person_this.neighbors;
        for (unsigned int n = 0u; n < neigh.size(); ++n)
        {
            // Point to the right neighbors
            int loc = res.persons_ids[neigh[n]->get_id()];
            person_res.add_neighbor(&res.persons[loc], true, true);

        }

    }

    return res;

}

#define EPIWORLD_CHECK_STATUS(a, b) \
    for (auto & i : b) \
        if (a == i) \
            throw std::logic_error("The status " + std::to_string(i) + " already exists."); 
#define EPIWORLD_CHECK_ALL_STATUSES(a) \
    EPIWORLD_CHECK_STATUS(a, status_susceptible) \
    EPIWORLD_CHECK_STATUS(a, status_infected) \
    EPIWORLD_CHECK_STATUS(a, status_removed)

template<typename TSeq>
inline void Model<TSeq>::add_status_susceptible(unsigned int s, std::string lab)
{

    EPIWORLD_CHECK_ALL_STATUSES(s)
    status_susceptible.push_back(s);
    status_susceptible_labels.push_back(lab);
    nstatus++;

}

template<typename TSeq>
inline void Model<TSeq>::add_status_infected(unsigned int s, std::string lab)
{

    EPIWORLD_CHECK_ALL_STATUSES(s)
    status_infected.push_back(s);
    status_infected_labels.push_back(lab);

}

template<typename TSeq>
inline void Model<TSeq>::add_status_removed(unsigned int s, std::string lab)
{

    EPIWORLD_CHECK_ALL_STATUSES(s)
    status_removed.push_back(s);
    status_removed_labels.push_back(lab);

}

template<typename TSeq>
inline void Model<TSeq>::add_status_susceptible(std::string lab)
{
    status_susceptible.push_back(nstatus++);
    status_susceptible_labels.push_back(lab);
}

template<typename TSeq>
inline void Model<TSeq>::add_status_infected(std::string lab)
{
    status_infected.push_back(nstatus++);
    status_infected_labels.push_back(lab);
}

template<typename TSeq>
inline void Model<TSeq>::add_status_removed(std::string lab)
{
    status_removed.push_back(nstatus++);
    status_removed_labels.push_back(lab);
}

#define EPIWORLD_COLLECT_STATUSES(out,id,lab) \
    std::vector< std::pair<unsigned int, std::string> > out; \
    for (unsigned int i = 0; i < id.size(); ++i) \
        out.push_back( \
            std::pair<int,std::string>( \
                id[i], lab[i] \
            ) \
        );

template<typename TSeq>
inline std::vector< std::pair<unsigned int,std::string> >
Model<TSeq>::get_status_susceptible() const
{

    EPIWORLD_COLLECT_STATUSES(
        res,
        status_susceptible,
        status_susceptible_labels
        )
    
    return res;
}

template<typename TSeq>
inline std::vector< std::pair<unsigned int,std::string> >
Model<TSeq>::get_status_infected() const
{
    EPIWORLD_COLLECT_STATUSES(
        res,
        status_infected,
        status_infected_labels
        )

    return res;
}

template<typename TSeq>
inline std::vector< std::pair<unsigned int,std::string> >
Model<TSeq>::get_status_removed() const
{
    EPIWORLD_COLLECT_STATUSES(
        res,
        status_removed,
        status_removed_labels
        )

    return res;
}

#undef EPIWORLD_CHECK_STATE
#undef EPIWORLD_CHECK_ALL_STATES
#undef EPIWORLD_COLLECT_STATUSES
#undef NEXT_STATUS

#undef CHECK_INIT
#endif