<?php

/**
 * @file
 * The taxonomy specific translation functions and hook implementations.
 */

/**
 * Returns whether the given taxonomy vocabulary has support for translations.
 *
 * @return bool
 *   TRUE if translation is enabled, FALSE otherwise.
 */
function entity_translation_taxonomy_term_enabled_vocabulary($vocabulary_name) {
  $info = variable_get('entity_translation_taxonomy', array());
  return !empty($info[$vocabulary_name]);
}

/**
 * Taxonomy-term-specific menu alterations.
 */
function entity_translation_taxonomy_term_menu_alter(&$items, $backup) {
  if (isset($backup['taxonomy_term'])) {
    $item = $backup['taxonomy_term'];
    // Preserve the menu router item defined by other modules.
    $callback['page callback'] = $item['page callback'];
    $callback['file'] = $item['file'];
    $callback['module'] = $item['module'];
    $access_arguments = array_merge(array(2, $item['access callback']), $item['access arguments']);
    $page_arguments = array_merge(array('taxonomy_term', 2, $callback), $item['page arguments']);
  }
  else {
    $access_arguments = array(2);
    $page_arguments = array('taxonomy_term', 2);
  }

  $items['taxonomy/term/%taxonomy_term/translate']['page callback'] = 'entity_translation_overview';
  $items['taxonomy/term/%taxonomy_term/translate']['page arguments'] = $page_arguments;
  $items['taxonomy/term/%taxonomy_term/translate']['access arguments'] = $access_arguments;
  $items['taxonomy/term/%taxonomy_term/translate']['access callback'] = 'entity_translation_taxonomy_term_tab_access';
  $items['taxonomy/term/%taxonomy_term/translate']['file'] = 'entity_translation.admin.inc';
  $items['taxonomy/term/%taxonomy_term/translate']['module'] = 'entity_translation';

  // Delete translation callback.
  $items['taxonomy/term/%taxonomy_term/translate/delete/%entity_translation_language']['access arguments'] = $access_arguments;
}

/**
 * Taxonomy term specific access callback.
 */
function entity_translation_taxonomy_term_tab_access() {
  $args = func_get_args();
  $term = array_shift($args);
  if (entity_translation_enabled('taxonomy_term', $term)) {
    return entity_translation_tab_access('taxonomy_term', $term);
  }
  else {
    $function = array_shift($args);
    return $function ? call_user_func_array($function, $args) : FALSE;
  }
}

/**
 * Implements hook_form_FORM_ID_alter()
 */
function entity_translation_form_taxonomy_form_vocabulary_alter(&$form, &$form_state) {
  if (entity_translation_enabled('taxonomy_term')) {
    $name = $form_state['vocabulary']->machine_name;
    if (isset($form['i18n_translation']['i18n_mode'])) {
      $args = array('!url' => url('admin/config/regional/entity_translation'));
      $form['i18n_translation']['i18n_mode']['#options'][I18N_MODE_ENTITY_TRANSLATION] = t('Field translation. Term fields will be translated through the <a href="!url">Entity translation</a> module.', $args);
      if (entity_translation_enabled_bundle('taxonomy_term', $name)) {
        $form['i18n_translation']['i18n_mode']['#default_value'] = I18N_MODE_ENTITY_TRANSLATION;
      }
    }
    else {
      $form['entity_translation_taxonomy'] = array(
        '#title' => t('Enable field translation'),
        '#type' => 'checkbox',
        '#prefix' => '<label>' . t('Translation') . '</label>',
        '#default_value' => entity_translation_enabled('taxonomy_term', $name),
      );
    }
    $form['#submit'][] = 'entity_translation_form_taxonomy_form_vocabulary_submit';
  }
}

/**
 * Submit handler for the taxonomy vocabulary form.
 */
function entity_translation_form_taxonomy_form_vocabulary_submit($form, &$form_state) {
  if (!empty($form_state['values']['i18n_mode']) && $form_state['values']['i18n_mode'] == I18N_MODE_ENTITY_TRANSLATION) {
    $form_state['values']['entity_translation_taxonomy'] = TRUE;
  }
  $info = variable_get('entity_translation_taxonomy', array());
  $info[$form_state['vocabulary']->machine_name] = !empty($form_state['values']['entity_translation_taxonomy']);
  variable_set('entity_translation_taxonomy', $info);
}

/**
 * Implements hook_field_widget_WIDGET_TYPE_form_alter().
 */
function entity_translation_field_widget_taxonomy_autocomplete_form_alter(&$element, &$form_state, $context) {
  // Ensure autocomplete taxonomy terms are created in the language of the
  // parent entity.
  $field = field_info_field($element['#field_name']);
  if (!empty($field['settings']['allowed_values'][0]['vocabulary'])) {
    $bundle = $field['settings']['allowed_values'][0]['vocabulary'];
    $entity_type = 'taxonomy_term';
    if (entity_translation_enabled($entity_type, $bundle)) {
      $handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
      $langcode = $handler->getActiveLanguage();
      // FIXME optional Title support
      $element['#autocomplete_path'] = 'entity_translation/' . $entity_type . '/autocomplete/' . $langcode . '/' . $element['#field_name'];
      $element['#element_validate'] = array_values(array_diff($element['#element_validate'], array('taxonomy_autocomplete_validate')));
      $element['#element_validate'][] = 'entity_translation_taxonomy_autocomplete_validate';
    }
  }
}

/**
 * TODO
 */
function entity_translation_taxonomy_term_autocomplete($langcode = NULL, $field_name = '', $tags_typed = '') {
  // If the request has a '/' in the search text, then the menu system will have
  // split it into multiple arguments, recover the intended $tags_typed.
  $args = func_get_args();
  // Shift off the $langcode and $field_name arguments.
  array_shift($args);
  array_shift($args);
  $tags_typed = implode('/', $args);

  // Make sure the field exists and is a taxonomy field.
  if (!($field = field_info_field($field_name)) || $field['type'] !== 'taxonomy_term_reference') {
    // Error string. The JavaScript handler will realize this is not JSON and
    // will display it as debugging information.
    // FIXME
    print t('Taxonomy field @field_name not found.', array('@field_name' => $field_name));
    exit;
  }

  // The user enters a comma-separated list of tags. We only autocomplete the last tag.
  $tags_typed = drupal_explode_tags($tags_typed);
  $tag_last = drupal_strtolower(array_pop($tags_typed));

  $term_matches = array();
  if ($tag_last != '') {
    if (!$langcode) {
      $langcode = $GLOBALS['language_content']->language;
    }

    // Part of the criteria for the query come from the field's own settings.
    $vids = array();
    $vocabularies = taxonomy_vocabulary_get_names();
    foreach ($field['settings']['allowed_values'] as $tree) {
      $vids[] = $vocabularies[$tree['vocabulary']]->vid;
    }

    $name_field = 'name_field';
    $entity_type = 'taxonomy_term';
    $query = new EntityFieldQuery();
    $query->addTag('taxonomy_term_access');
    $query->entityCondition('entity_type', $entity_type);
    $query->fieldLanguageCondition($name_field, $langcode);

    // Do not select already entered terms.
    if (!empty($tags_typed)) {
      $query->fieldCondition($name_field, NULL, $tags_typed, 'NOT IN');
    }

    // Select rows that match by term name.
    $query->propertyCondition('vid', $vids);
    $query->fieldCondition($name_field, NULL, $tag_last, 'CONTAINS');
    $query->range(0, 10);
    $result = $query->execute();

    $prefix = count($tags_typed) ? drupal_implode_tags($tags_typed) . ', ' : '';
    $terms = !empty($result[$entity_type]) ? taxonomy_term_load_multiple(array_keys($result[$entity_type])) : array();

    foreach ($terms as $tid => $term) {
      $name = entity_label('taxonomy_term', $term);
      $n = $name;
      // Term names containing commas or quotes must be wrapped in quotes.
      if (strpos($name, ',') !== FALSE || strpos($name, '"') !== FALSE) {
        $n = '"' . str_replace('"', '""', $name) . '"';
      }
      $term_matches[$prefix . $n] = check_plain($name);
    }
  }

  drupal_json_output($term_matches);
}

/**
 * Form element validate handler for taxonomy term autocomplete element.
 */
function entity_translation_taxonomy_autocomplete_validate($element, &$form_state) {
  // Autocomplete widgets do not send their tids in the form, so we must detect
  // them here and process them independently.
  $value = array();
  if ($tags = $element['#value']) {
    // Collect candidate vocabularies.
    $field = field_widget_field($element, $form_state);
    $vocabularies = array();
    foreach ($field['settings']['allowed_values'] as $tree) {
      if ($vocabulary = taxonomy_vocabulary_machine_name_load($tree['vocabulary'])) {
        $vocabularies[$vocabulary->vid] = $vocabulary;
      }
    }
    $vocabulary = reset($vocabularies);

    // TODO
    $entity_type = 'taxonomy_term';
    $name_field = 'name_field';
    $typed_tags = drupal_explode_tags($tags);

    // This is the language of the parent entity, that we will be appling to
    // terms. TODO
    $parent_handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
    $langcode = $parent_handler->getActiveLanguage();
    $langcode_key = $parent_handler->getLanguageKey();
    $original_langcode = !empty($form_state['values'][$langcode_key]) ? $form_state['values'][$langcode_key] : $langcode;

    // Select terms that match by (translated) term name.
    $query = new EntityFieldQuery();
    $query->addTag('taxonomy_term_access');
    $query->entityCondition('entity_type', $entity_type);
    $query->propertyCondition('vid', $vocabulary->vid);
    $query->fieldLanguageCondition($name_field, $langcode);
    $query->fieldCondition($name_field, NULL, $typed_tags);
    $result = $query->execute();

    $translated_terms = array();
    if (!empty($result[$entity_type])) {
      foreach (taxonomy_term_load_multiple(array_keys($result[$entity_type])) as $term) {
        $name = entity_label($entity_type, $term);
        $translated_terms[$name] = $term;
      }
    }

    // TODO
    $source_langcode = NULL;
    $source_values = array();
    $access = user_access('translate any entity') || user_access("translate $this->entityType entities");
    if ($access) {
      $translations = $parent_handler->getTranslations();
      $source_langcode = isset($translations->data[$langcode]) ? $translations->data[$langcode]['source'] : $parent_handler->getSourceLanguage();
      if (!empty($element['#entity']->{$element['#field_name']}[LANGUAGE_NONE])) { // FIXME
        $tids = array_map(function ($item) { return $item['tid']; }, $element['#entity']->{$element['#field_name']}[LANGUAGE_NONE]);
        if (count($tids) == count($typed_tags)) {
          $source_values = array_values(taxonomy_term_load_multiple($tids));
        }
      }
    }

    // Translate term names into actual terms.
    foreach ($typed_tags as $delta => $typed_tag) {
      // TODO
      // See if the term exists in the chosen vocabulary and return the tid;
      // otherwise, create a new 'autocreate' term for insert/update.
      if (isset($translated_terms[$typed_tag])) {
        $term = $translated_terms[$typed_tag];
      }
      elseif ($access && isset($source_values[$delta])) {
        $term = $source_values[$delta];
        $handler = entity_translation_get_handler($entity_type, $term);
        $translation = array(
          'language' => $langcode,
          'source' => $source_langcode,
          'uid' => $GLOBALS['user']->uid,
          'status' => 1,
          'created' => REQUEST_TIME,
          'changed' => REQUEST_TIME,
        );
        $translation_values = array(
          $name_field => array($langcode => array(array('value' => $typed_tag))),
        );
        $handler->setTranslation($translation, $translation_values);
        entity_translation_entity_save($entity_type, $term);
      }
      else {
        $term = (object) array(
          'tid' => 'autocreate',
          'vid' => $vocabulary->vid,
          'name' => $typed_tag,
          'vocabulary_machine_name' => $vocabulary->machine_name,
        );
        $handler = entity_translation_get_handler('taxonomy_term', $term);
        $handler->setOriginalLanguage($original_langcode);
        $handler->initTranslations();
      }
      $value[] = (array) $term;
    }
  }

  form_set_value($element, $value, $form_state);
}

/**
 * Add the parent entity language to autocomplete taxonomy terms before they are
 * created.
 */
function _entity_translation_taxonomy_autocomplete_validate($element, &$form_state) {
  $parent_handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
  $langcode_key = $parent_handler->getLanguageKey();
  if (!empty($form_state['values'][$langcode_key])) {
    // This is the language of the parent entity, that we will be appling to
    // terms.
    $original_langcode = $form_state['values'][$langcode_key];
    // Fetch the field values, this also works for nested values, such as field
    // collections.
    $value = drupal_array_get_nested_value($form_state['values'], $element['#parents']);
    foreach ($value as $delta => $term) {
      if ($term['tid'] === 'autocreate') {
        // Using the "autocreate" term created by taxonomy module, we create a
        // temporary object and initiate the original translation in the parent
        // entity language.
        $term = (object) $term;
        $handler = entity_translation_get_handler('taxonomy_term', $term);
        $handler->setOriginalLanguage($original_langcode);
        $handler->initTranslations();
        // Apply the language and translation structures to the original
        // "autocreate" term element.
        $value[$delta]['language'] = $original_langcode;
        $value[$delta]['translations'] = $term->translations;
      }
    }
    // Update form values.
    form_set_value($element, $value, $form_state);
  }
}
