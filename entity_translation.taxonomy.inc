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
 * TODO
 */
function entity_translation_taxonomy_label($term, $langcode) {
  $entity_type = 'taxonomy_term';
  if (function_exists('title_entity_label')) {
    return title_entity_label($term, $entity_type, $langcode);
  }
  else {
    return entity_label($entity_type, $term);
  }
}

/**
 * Implements entity_translation_form_field_ui_field_edit_WIDGET_TYPE_form_alter().
 *
 * {@inheritdoc}
 */
function entity_translation_form_field_ui_field_edit_taxonomy_autocomplete_form_alter(&$form, &$form_state) {
  $key = 'entity_translation_taxonomy_autocomplete_translate';
  $instance = $form['#instance'];
  $field_name = $instance['field_name'];
  $entity_type = $instance['entity_type'];
  $field = field_info_field($field_name);
  $translatable = field_is_translatable($entity_type, $field);

  $form['instance']['settings'][$key] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable in-place translation of terms'),
    '#description' => t('Check this option if you wish to use translation forms to perform in-place translation for terms entered in the original language.'),
    '#default_value' => !$translatable && !empty($form['#instance']['settings'][$key]),
    '#access' => !$form_state['field_has_data'] || !$translatable,
    '#states' => array(
      'visible' => array(':input[name="field[translatable]"]' => array('checked' => FALSE)),
    ),
    '#weight' => -8,
  );
}

/**
 * TODO
 */
function entity_translation_taxonomy_autocomplete_translation_enabled($element) {
  $field = field_info_field($element['#field_name']);
  if (field_is_translatable($element['#entity_type'], $field)) {
    return FALSE;
  }

  $entity_type = 'taxonomy_term';
  $parent_handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
  $langcode = $parent_handler->getActiveLanguage();
  $translations = $parent_handler->getTranslations();
  list(,, $bundle) = entity_extract_ids($element['#entity_type'], $element['#entity']);
  $instance = field_info_instance($element['#entity_type'], $field['field_name'], $bundle);

  return
    isset($translations->original) && $translations->original != $langcode &&
    !empty($instance['settings']['entity_translation_taxonomy_autocomplete_translate']) &&
    (user_access('translate any entity') || user_access("translate $entity_type entities"));
}

/**
 * Implements hook_field_widget_WIDGET_TYPE_form_alter().
 */
function entity_translation_field_widget_taxonomy_autocomplete_form_alter(&$element, &$form_state, $context) {
  // TODO
  if (!isset($element['#entity'])) {
    return;
  }

  // Ensure autocomplete taxonomy terms are created in the language of the
  // parent entity. TODO
  $entity_type = 'taxonomy_term';
  $field = field_widget_field($element, $form_state);
  $bundle = !empty($field['settings']['allowed_values'][0]['vocabulary']) ? $field['settings']['allowed_values'][0]['vocabulary'] : NULL;
  if ($bundle && entity_translation_enabled($entity_type, $bundle)) {
    // TODO
    $element['#element_validate'] = array_values(array_diff($element['#element_validate'], array('taxonomy_autocomplete_validate')));
    $element['#element_validate'][] = 'entity_translation_taxonomy_autocomplete_validate';

    // TODO
    $parent_handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
    $langcode = $parent_handler->getActiveLanguage();
    $translations = $parent_handler->getTranslations();
    $items = isset($element['#entity']->{$element['#field_name']}[$element['#language']]) ?
      $element['#entity']->{$element['#field_name']}[$element['#language']] : array();
    $tids = array_map(function ($item) { return $item['tid']; }, $items);
    $terms = array_values(taxonomy_term_load_multiple($tids));

    // TODO
    if (!entity_translation_taxonomy_autocomplete_translation_enabled($element)) {
      $element['#autocomplete_path'] = 'entity_translation/' . $entity_type . '/autocomplete/' . $langcode . '/' . $element['#field_name'];
      if (isset($translations->original) && $translations->original != $langcode) {
        $labels = array();
        foreach ($terms as $delta => $term) {
          $labels[] = entity_translation_taxonomy_label($term, $langcode);
        }
        $element['#default_value'] = implode(', ', $labels);
      }
    }
    // TODO
    else {
      $element['#type'] = 'fieldset';
      $element['#description'] = t('Enter one translation for each term');
      $element['#access'] = (bool) $terms;
      foreach ($terms as $delta => $term) {
        $element[$delta] = array(
          '#type' => 'textfield',
          '#default_value' => entity_translation_taxonomy_label($term, $langcode),
          '#required' => TRUE,
          '#tid' => $term->tid,
        );
      }
      $element['#process'][] = 'entity_translation_taxonomy_autocomplete_process';
    }
  }
}

/**
 * TODO
 * @param $element
 * @return mixed
 */
function entity_translation_taxonomy_autocomplete_process($element) {
  // TODO
  $instance = field_info_instance($element['#entity_type'], $element['#field_name'], $element['#bundle']);
  $element['#title'] = check_plain($instance['label']);
  return $element;
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
    print t('Taxonomy field @field_name not found.', array('@field_name' => $field_name));
    exit;
  }

  // The user enters a comma-separated list of tags. We only autocomplete the last tag.
  $tags_typed = drupal_explode_tags($tags_typed);
  $tag_last = drupal_strtolower(array_pop($tags_typed));

  $term_matches = array();
  if ($tag_last != '') {
    if (!isset($langcode) || $langcode == LANGUAGE_NONE) {
      $langcode = $GLOBALS['language_content']->language;
    }

    // Part of the criteria for the query come from the field's own settings.
    $vids = array();
    $vocabularies = taxonomy_vocabulary_get_names();
    foreach ($field['settings']['allowed_values'] as $tree) {
      $vids[] = $vocabularies[$tree['vocabulary']]->vid;
    }
    $vocabulary = reset($vocabularies);

    $entity_type = 'taxonomy_term';
    $query = new EntityFieldQuery();
    $query->addTag('taxonomy_term_access');
    $query->entityCondition('entity_type', $entity_type);

    // If the Title module is enabled and the taxonomy term name is replaced for
    // the current bundle, we can look for translated names, otherwise we fall
    // back to the regular name property.
    if (module_invoke('title', 'field_replacement_enabled', $entity_type, $vocabulary->machine_name, 'name')) {
      $name_field = 'name_field';
      $language_group = 0;
      // Do not select already entered terms.
      $column = 'value';
      if (!empty($tags_typed)) {
        $query->fieldCondition($name_field, $column, $tags_typed, 'NOT IN', NULL, $language_group);
      }
      $query->fieldCondition($name_field, $column, $tag_last, 'CONTAINS', NULL, $language_group);
      $query->fieldLanguageCondition($name_field, array($langcode, LANGUAGE_NONE), NULL, NULL, $language_group);
    }
    else {
      $name_field = 'name';
      // Do not select already entered terms.
      if (!empty($tags_typed)) {
        $query->propertyCondition($name_field, $tags_typed, 'NOT IN');
      }
      $query->propertyCondition($name_field, $tag_last, 'CONTAINS');
    }

    // Select rows that match by term name.
    $query->propertyCondition('vid', $vids);
    $query->range(0, 10);
    $result = $query->execute();

    // Populate the results array.
    $prefix = count($tags_typed) ? drupal_implode_tags($tags_typed) . ', ' : '';
    $terms = !empty($result[$entity_type]) ? taxonomy_term_load_multiple(array_keys($result[$entity_type])) : array();
    foreach ($terms as $tid => $term) {
      $name = entity_translation_taxonomy_label($term, $langcode);
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
  $value = array();

  // This is the language of the parent entity, that we will be appling to
  // terms. TODO
  $parent_handler = entity_translation_get_handler($element['#entity_type'], $element['#entity']);
  $langcode = $parent_handler->getActiveLanguage();
  list($id) = entity_extract_ids($element['#entity_type'], $element['#entity']);
  $is_new = !isset($id);

  if (entity_translation_taxonomy_autocomplete_translation_enabled($element)) {
    $deltas = array();
    foreach (element_children($element) as $delta) {
      $tid = $element[$delta]['#tid'];
      $deltas[$tid] = $delta;
      $value[$delta]['tid'] = $tid;
    }

    // TODO
    $entity_type = 'taxonomy_term';
    $name_field = 'name_field'; // FIXME
    $source_langcode = $parent_handler->getSourceLanguage();
    foreach (taxonomy_term_load_multiple(array_keys($deltas)) as $term) {
      // TODO
      $term_translation = $element[$deltas[$term->tid]]['#value'];
      if (!$term_translation) {
        $instance = field_info_instance($element['#entity_type'], $element['#field_name'], $element['#bundle']);
        drupal_set_message(t('The translations for %field_label cannot be empty.', array('%field_label' => $instance['label'])), 'error', FALSE);
        continue;
      }

      $handler = entity_translation_get_handler($entity_type, $term);
      $translations = $handler->getTranslations();
      $term_langcode = $handler->getLanguage();
      $active_langcode = $term_langcode != LANGUAGE_NONE ? $langcode : $term_langcode;

      // TODO
      if (!isset($translations->data[$active_langcode]) && $active_langcode != LANGUAGE_NONE) {
        $translation = array(
          'language' => $active_langcode,
          'source' => $source_langcode,
          'uid' => $GLOBALS['user']->uid,
          'status' => 1,
          'created' => REQUEST_TIME,
          'changed' => REQUEST_TIME,
        );
        $translation_values = array(
          $name_field => array($active_langcode => array(array('value' => $term_translation))),
        );
        $handler->setTranslation($translation, $translation_values);
        entity_translation_entity_save($entity_type, $term);
      }
      // TODO
      elseif ($term_translation != entity_translation_taxonomy_label($term, $active_langcode)) {
        $term->{$name_field}[$active_langcode][0]['value'] = $term_translation;
        entity_translation_entity_save($entity_type, $term);
      }
    }
  }
  // Autocomplete widgets do not send their tids in the form, so we must detect
  // them here and process them independently.
  elseif ($tags = $element['#value']) {
    $field = field_widget_field($element, $form_state);
    // FIXME function
    $vocabularies = array();
    foreach ($field['settings']['allowed_values'] as $tree) {
      if ($vocabulary = taxonomy_vocabulary_machine_name_load($tree['vocabulary'])) {
        $vocabularies[$vocabulary->vid] = $vocabulary;
        break;
      }
    }

    $entity_type = 'taxonomy_term';
    $vocabulary = reset($vocabularies);
    $typed_tags = drupal_explode_tags($tags);

    // FIXME function
    $items = isset($element['#entity']->{$element['#field_name']}[$element['#language']]) ?
      $element['#entity']->{$element['#field_name']}[$element['#language']] : array();
    $tids = array_map(function ($item) { return $item['tid']; }, $items);

    $existing_terms = array();
    foreach (taxonomy_term_load_multiple($tids) as $term) {
      $name = entity_translation_taxonomy_label($term, $langcode);
      $existing_terms[$name] = $term;
    }

    // Select terms that match by the (translated) name.
    $query = new EntityFieldQuery();
    $query->addTag('taxonomy_term_access');
    $query->entityCondition('entity_type', $entity_type);
    $query->propertyCondition('vid', $vocabulary->vid);
    if ($langcode != LANGUAGE_NONE && module_invoke('title', 'field_replacement_enabled', $entity_type, $vocabulary->machine_name, 'name')) {
      $language_group = 0;
      // Do not select already entered terms.
      $name_field = 'name_field';
      $column = 'value';
      $query->fieldCondition($name_field, $column, $typed_tags, NULL, NULL, $language_group);
      // TODO
      if (!$is_new) {
        $query->fieldLanguageCondition($name_field, array($langcode, LANGUAGE_NONE), NULL, NULL, $language_group);
      }
    }
    else {
      $query->propertyCondition('name', $typed_tags);
    }
    $result = $query->execute();
    if (!empty($result[$entity_type])) {
      foreach (taxonomy_term_load_multiple(array_keys($result[$entity_type])) as $term) {
        $name = entity_translation_taxonomy_label($term, $langcode);
        $existing_terms[$name] = $term;
        // TODO
        if ($is_new) {
          $name = entity_translation_taxonomy_label($term, $GLOBALS['language_content']->language);
          $existing_terms[$name] = $term;
        }
      }
    }

    // FIXME: language change
    // Translate term names into actual terms.
    foreach ($typed_tags as $delta => $typed_tag) {
      // See if the term exists in the chosen vocabulary and return the tid.
      // Otherwise create a new 'autocreate' term for insert/update.
      if (isset($existing_terms[$typed_tag])) {
        $term = $existing_terms[$typed_tag];
      }
      else {
        $term = (object) array(
          'tid' => 'autocreate',
          'vid' => $vocabulary->vid,
          'name' => $typed_tag,
          'vocabulary_machine_name' => $vocabulary->machine_name,
        );
        $handler = entity_translation_get_handler($entity_type, $term);
        $handler->setOriginalLanguage($langcode);
        $handler->initTranslations();
      }
      $value[] = (array) $term;
    }
  }

  form_set_value($element, $value, $form_state);
}
