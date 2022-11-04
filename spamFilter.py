############################################################
# CMPSC442: Homework 5
############################################################

student_name = "Gabien Bryan"

############################################################
# Imports
############################################################

import math
import email
import os
import itertools
import collections

############################################################
# Section 1: Spam Filter
############################################################

def load_tokens(email_path):
    email_file = open(email_path)
    email_msg = email.message_from_file(email_file) # outlined in hw description
    email_lines = email.iterators.body_line_iterator(email_msg) # outlined in hw description
    tokens_list = []
    for email_line in email_lines:
        tokens_list = tokens_list + email_line.split() # use split to seperate words in each line and add to token list
    return tokens_list
    

def log_probs(email_paths, smoothing):
    words_dict = {}
    tokens = []
    for email_path in email_paths:
        tokens = load_tokens(email_path) # put all tokens from each path into one list
        for token in tokens:
            if token in words_dict: # check to see if token is in dict, kept getting "KeyError" without this if statement
                words_dict[token] += 1
            else:
                words_dict[token] = 1
    P_dict = {} # dictionary for log values
    for word,count in words_dict.items(): # Pull out dict values to use in equations and as indexes for log dict
        # defined equation for P(w) from instructions
        P_dict[word] = math.log((count + smoothing)/(sum(words_dict.values()) + smoothing * (len(words_dict) + 1)))
    # defined equation for P(<UNK>) from instructions
    P_dict["<UNK>"] = math.log(smoothing/(sum(words_dict.values()) + smoothing * (len(words_dict) + 1)))
    return P_dict        

class SpamFilter(object):

    def __init__(self, spam_dir, ham_dir, smoothing):
        self.spam_log_dict = {}
        self.ham_log_dict = {}
        spam_paths = []
        ham_paths = []
        # runtime is terrible, I tried a few different ways of doing this with os.listdir and other functions but couldnt improve time
        for (path, dirs, files) in os.walk(spam_dir): # os.walk goes through directory topdown
            for f in files: # iterate by each file
                # get full file path
                temp = os.path.join(path,f)
                # join file path so list is not seperated by each char
                file = ''.join(temp)
                spam_paths.append(file) # tried to use os.path.join() here but spam_paths had each char as an index
        for path, dirs, files in os.walk(ham_dir): # os.walk goes through directory topdown
            for f in files: # iterate by each file
                # get full file path
                temp = os.path.join(path,f)
                # join file path so list is not seperated by each char
                file = ''.join(temp)
                ham_paths.append(file) # tried to use os.path.join() here but spam_paths had each char as an index
        # store log probs for spam in dict for internall use
        self.spam_log_dict = log_probs(spam_paths, smoothing)
        # store log probs for ham in dict for internall use
        self.ham_log_dict = log_probs(ham_paths, smoothing)
        # probability of file being spam file
        self.spam_P = math.log(len(spam_paths) / (len(ham_paths) + len(spam_paths)))
        # probability of file being ham file
        self.ham_P = math.log(len(ham_paths) / (len(ham_paths) + len(spam_paths)))
            
    def is_spam(self, email_path):
        tokens = load_tokens(email_path) # get words from email path
        #set initial probabilities
        spam_P = self.spam_P
        ham_P = self.ham_P
        for word in tokens: # iterate through all words
            # check to see if word is in the spam directory, if is add its log value, else add <UNK> value
            if word in self.spam_log_dict:
                spam_P += self.spam_log_dict[word]
            else:
                spam_P += self.spam_log_dict["<UNK>"]
            # check to see if word is in the ham directory, if is add its log value, else add <UNK> value
            if word in self.ham_log_dict:
                ham_P += self.ham_log_dict[word]
            else:
                ham_P += self.ham_log_dict["<UNK>"]
        # if spam_P >= ham_P then its spam so retrun True, else return False
        if spam_P > ham_P:
            return True
        else:
            return False

    def most_indicative_spam(self, n):
        m_indicative = {}
        sorted_ind = {}
        ret_list = []
        for word, count in self.ham_log_dict.items(): # iterate through all words in ham dict
            value = math.exp(self.ham_log_dict[word]) # get the count value for word from ham dict
            # see if word is also in spam dict
            if word in self.spam_log_dict:
                value += math.exp(self.spam_log_dict[word]) # add count value for word in spam dict to total value
            # check if word is already in indicative dict
            if word in m_indicative:
                m_indicative[word] += count - math.log(value) # log - log = log(a/b), this is the equation outlined in instructions
            else:
                m_indicative[word] = count - math.log(value) # log - log = log(a/b), this is the equation outlined in instructions
        # dictionary needs to be sorted in order to get the descending order of indication values
        sorted_ind = sorted(m_indicative.items(), key=lambda x: x[1])
        index = 1
        # place the first n words from sorted dict into a list to return 
        for word, count in sorted_ind:
            ret_list.append(word)
            if index >= n:
                return ret_list
            index += 1
        

    def most_indicative_ham(self, n):
        m_indicative = {}
        sorted_ind = {}
        ret_list = []
        for word, count in self.spam_log_dict.items(): # iterate through all words in spam dict
            value = math.exp(self.spam_log_dict[word]) # get the count value for word from spam dict
            # see if word is also in ham dict
            if word in self.ham_log_dict:
                value += math.exp(self.ham_log_dict[word]) # add count value for word in ham dict to total value
            # check if word is already in indicative dict
            if word in m_indicative:
                m_indicative[word] += count - math.log(value) # log - log = log(a/b), this is the equation outlined in instructions
            else:
                m_indicative[word] = count - math.log(value) # log - log = log(a/b), this is the equation outlined in instructions
        # dictionary needs to be sorted in order to get the descending order of indication values
        sorted_ind = sorted(m_indicative.items(), key=lambda x: x[1])
        index = 1
        # place the first n words from sorted dict into a list to return
        for word, count in sorted_ind:
            ret_list.append(word)
            if index >= n:
                return ret_list
            index += 1

